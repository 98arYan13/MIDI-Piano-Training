#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <sightreading/Tray.hpp>
namespace sightreading {
namespace {
struct Context { std::string url; std::atomic<bool> *stopping; };
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
  auto *context = reinterpret_cast<Context *>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_CREATE) {
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams));
    return 0;
  }
  if (message == WM_APP + 1 && context) {
    if (l == WM_LBUTTONDBLCLK) ShellExecuteA(nullptr, "open", context->url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (l == WM_RBUTTONUP || l == WM_CONTEXTMENU) {
      auto menu = CreatePopupMenu(); AppendMenuW(menu, MF_STRING, 1, L"Open Sight Reading"); AppendMenuW(menu, MF_STRING, 2, L"Quit and stop sound");
      POINT position; GetCursorPos(&position); SetForegroundWindow(window);
      auto choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, position.x, position.y, 0, window, nullptr);
      DestroyMenu(menu);
      if (choice == 1) ShellExecuteA(nullptr, "open", context->url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
      if (choice == 2) *context->stopping = true;
    }
    return 0;
  }
  if (message == WM_TIMER && context && context->stopping->load()) DestroyWindow(window);
  if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(window, message, w, l);
}
}
void runTray(const std::string &url, std::atomic<bool> &stopping) {
  Context context{url, &stopping};
  WNDCLASSW klass{}; klass.lpfnWndProc = procedure; klass.hInstance = GetModuleHandleW(nullptr); klass.lpszClassName = L"SightReadingTray";
  RegisterClassW(&klass);
  auto window = CreateWindowExW(0, klass.lpszClassName, L"Sight Reading", 0, 0, 0, 0, 0, nullptr, nullptr, klass.hInstance, &context);
  if (!window) return;
  NOTIFYICONDATAW icon{}; icon.cbSize = sizeof(icon); icon.hWnd = window; icon.uID = 1; icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; icon.uCallbackMessage = WM_APP + 1; icon.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
  lstrcpyW(icon.szTip, L"Sight Reading — MIDI and metronome remain active");
  Shell_NotifyIconW(NIM_ADD, &icon); SetTimer(window, 1, 250, nullptr);
  MSG message; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
  Shell_NotifyIconW(NIM_DELETE, &icon);
}
}
