#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>
#include <sstream>
#include <iostream>
namespace {
enum class Failure { None, Registration, Icon, Wait };
Failure failure{};
ATOM registerTray(const WNDCLASSW* klass){if(failure==Failure::Registration){SetLastError(ERROR_ACCESS_DENIED);return 0;}return RegisterClassW(klass);}
BOOL notifyTray(DWORD action,NOTIFYICONDATAW* icon){if(failure==Failure::Icon&&action==NIM_ADD){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}return Shell_NotifyIconW(action,icon);}
DWORD waitTray(DWORD count,const HANDLE* handles,DWORD timeout,DWORD mask,DWORD flags){if(failure==Failure::Wait){SetLastError(ERROR_ACCESS_DENIED);return WAIT_FAILED;}return MsgWaitForMultipleObjectsEx(count,handles,timeout,mask,flags);}
}
#define RegisterClassW registerTray
#define Shell_NotifyIconW notifyTray
#define MsgWaitForMultipleObjectsEx waitTray
#include "../src/Tray.cpp"
#undef RegisterClassW
#undef Shell_NotifyIconW
#undef MsgWaitForMultipleObjectsEx
TEST_CASE("Tray lifecycle reports failures and exits without timer delivery", "[final-tray]") {
 for(auto mode:{Failure::Registration,Failure::Icon,Failure::Wait}){
  failure=mode;std::atomic<bool> stopping{},done{};std::ostringstream diagnostic;auto previous=std::cerr.rdbuf(diagnostic.rdbuf());
  std::thread worker([&]{sightreading::runTray("http://127.0.0.1:8765",stopping);done=true;});
  for(unsigned i=0;i<100&&!done;i++)std::this_thread::sleep_for(std::chrono::milliseconds(10));
  bool returned=done;stopping=true;worker.join();std::cerr.rdbuf(previous);
  REQUIRE(returned);REQUIRE(diagnostic.str().find("Stop.cmd")!=std::string::npos);
 }
 failure=Failure::None;
}
