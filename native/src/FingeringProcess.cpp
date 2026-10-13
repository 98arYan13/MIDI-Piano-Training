#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <sightreading/FingeringProcess.hpp>
#include <thread>
#include <windows.h>
namespace sightreading {
namespace {
constexpr size_t cap = 256 * 1024 * 1024;
std::wstring quote(const std::wstring &s) {
  std::wstring out = L"\"";
  unsigned slashes = 0;
  for (auto c : s) {
    if (c == L'\\') {
      ++slashes;
      continue;
    }
    if (c == L'"')
      out.append(slashes * 2 + 1, L'\\');
    else
      out.append(slashes, L'\\');
    slashes = 0;
    out += c;
  }
  out.append(slashes * 2, L'\\');
  return out + L"\"";
}
void close(HANDLE &h) {
  if (h && h != INVALID_HANDLE_VALUE)
    CloseHandle(h);
  h = nullptr;
}
} // namespace
struct FingeringProcess::Impl {
  HANDLE child{}, job{}, input{}, output{}, err{};
  std::thread reader, errors;
  mutable std::mutex mutex;
  std::condition_variable changed;
  std::deque<unsigned char> bytes;
  std::string stderrText, failure;
  bool eof{};
  std::atomic<bool> stopped{};
  ~Impl() {
    stop();
    if (reader.joinable())
      reader.join();
    if (errors.joinable())
      errors.join();
    if (child)
      WaitForSingleObject(child, 5000);
    close(input);
    close(output);
    close(err);
    close(child);
    close(job);
  }
  void stop() {
    if (!stopped.exchange(true) && job)
      TerminateJobObject(job, 1);
    changed.notify_all();
  }
  void read(HANDLE h, bool diagnostic) {
    char buffer[8192];
    DWORD count;
    while (ReadFile(h, buffer, sizeof buffer, &count, nullptr) && count) {
      std::unique_lock lock(mutex);
      if (diagnostic) {
        stderrText.append(buffer, count);
        if (stderrText.size() > 4096)
          stderrText.erase(0, stderrText.size() - 4096);
      } else {
        if (bytes.size() + count > cap + 4) {
          failure = "Worker output exceeded frame buffer limit";
          stop();
          break;
        }
        bytes.insert(bytes.end(), buffer, buffer + count);
      }
      changed.notify_all();
    }
    if (!diagnostic) {
      std::lock_guard lock(mutex);
      eof = true;
      changed.notify_all();
    }
  }
};
FingeringProcess::FingeringProcess(const std::filesystem::path &exe,
                                   const std::vector<std::wstring> &args)
    : impl(std::make_unique<Impl>()) {
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE inRead{}, outWrite{}, errWrite{};
  auto cleanup = [&] {
    close(inRead);
    close(outWrite);
    close(errWrite);
  };
  if (!CreatePipe(&inRead, &impl->input, &sa, 0) ||
      !CreatePipe(&impl->output, &outWrite, &sa, 0) ||
      !CreatePipe(&impl->err, &errWrite, &sa, 0)) {
    cleanup();
    throw std::runtime_error("Cannot create fingering worker pipes");
  }
  SetHandleInformation(impl->input, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(impl->output, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(impl->err, HANDLE_FLAG_INHERIT, 0);
  SIZE_T size = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
  std::vector<unsigned char> attributes(size);
  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof startup;
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = inRead;
  startup.StartupInfo.hStdOutput = outWrite;
  startup.StartupInfo.hStdError = errWrite;
  startup.lpAttributeList =
      reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
  HANDLE inherited[] = {inRead, outWrite, errWrite};
  if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0,
                                         &size)) {
    cleanup();
    throw std::runtime_error("Cannot initialize restricted worker inheritance");
  }
  const bool updated = UpdateProcThreadAttribute(
      startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited,
      sizeof inherited, nullptr, nullptr);
  impl->job = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  bool configured =
      impl->job &&
      SetInformationJobObject(impl->job, JobObjectExtendedLimitInformation,
                              &limits, sizeof limits);
  std::wstring command = quote(exe.wstring());
  for (auto &a : args)
    command += L" " + quote(a);
  PROCESS_INFORMATION process{};
  bool created =
      updated && configured &&
      CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                     CREATE_NO_WINDOW | CREATE_SUSPENDED |
                         EXTENDED_STARTUPINFO_PRESENT,
                     nullptr, nullptr, &startup.StartupInfo, &process);
  DeleteProcThreadAttributeList(startup.lpAttributeList);
  cleanup();
  if (!created)
    throw std::runtime_error(
        "Cannot launch local fingering runtime; run setup-fingering.ps1");
  impl->child = process.hProcess;
  if (!AssignProcessToJobObject(impl->job, impl->child)) {
    TerminateProcess(impl->child, 1);
    CloseHandle(process.hThread);
    throw std::runtime_error("Cannot own fingering worker job");
  }
  ResumeThread(process.hThread);
  CloseHandle(process.hThread);
  impl->reader = std::thread([this] { impl->read(impl->output, false); });
  impl->errors = std::thread([this] { impl->read(impl->err, true); });
}
FingeringProcess::~FingeringProcess() = default;
void FingeringProcess::terminate() { impl->stop(); }
std::string FingeringProcess::diagnostics() const {
  std::lock_guard lock(impl->mutex);
  return impl->stderrText;
}
void FingeringProcess::writeFrame(const nlohmann::json &value) {
  auto body = value.dump();
  if (body.size() > cap)
    throw std::invalid_argument("Fingering request exceeds 256 MiB");
  std::string frame(4, '\0');
  for (unsigned i = 0; i < 4; ++i)
    frame[i] = char(body.size() >> (i * 8));
  frame += body;
  std::mutex mutex;
  std::condition_variable done;
  bool finished = false;
  std::atomic<bool> ok{true};
  std::thread writer([&] {
    size_t offset = 0;
    while (offset < frame.size()) {
      DWORD written = 0;
      if (!WriteFile(impl->input, frame.data() + offset,
                     DWORD(std::min<size_t>(8192, frame.size() - offset)),
                     &written, nullptr) ||
          !written) {
        ok = false;
        break;
      }
      offset += written;
    }
    std::lock_guard lock(mutex);
    finished = true;
    done.notify_one();
  });
  {
    std::unique_lock lock(mutex);
    if (!done.wait_for(lock, std::chrono::seconds(30),
                       [&] { return finished; })) {
      impl->stop();
      CancelSynchronousIo(writer.native_handle());
      ok = false;
    }
  }
  writer.join();
  if (!ok)
    throw std::runtime_error("Worker request pipe closed or timed out");
}
std::optional<nlohmann::json>
FingeringProcess::readFrame(std::chrono::milliseconds timeout) {
  std::unique_lock lock(impl->mutex);
  auto end = std::chrono::steady_clock::now() + timeout;
  for (;;) {
    if (!impl->failure.empty())
      throw std::runtime_error(impl->failure);
    if (impl->bytes.size() >= 4) {
      uint32_t length = 0;
      for (unsigned i = 0; i < 4; ++i)
        length |= uint32_t(impl->bytes[i]) << (8 * i);
      if (!length || length > cap)
        throw std::runtime_error(
            "Invalid worker frame length (maximum 256 MiB)");
      if (impl->bytes.size() >= size_t(length) + 4) {
        for (int i = 0; i < 4; ++i)
          impl->bytes.pop_front();
        std::string body;
        body.reserve(length);
        for (uint32_t i = 0; i < length; ++i) {
          body += char(impl->bytes.front());
          impl->bytes.pop_front();
        }
        return nlohmann::json::parse(body);
      }
    }
    if (impl->eof || impl->stopped)
      throw std::runtime_error(impl->bytes.empty() ? "Worker pipe closed"
                                                   : "Truncated worker frame");
    if (impl->changed.wait_until(lock, end) == std::cv_status::timeout)
      return std::nullopt;
  }
}
} // namespace sightreading
