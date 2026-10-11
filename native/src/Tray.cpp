#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <sightreading/Tray.hpp>
#include <iostream>
namespace sightreading {
namespace {
void diagnostic(const char* operation) { std::cerr << "Tray " << operation << " failed (Windows " << GetLastError() << "). Sight Reading keeps running; use Stop.cmd to stop sound and exit.\n"; }
void openBrowser(const std::string& url){if(reinterpret_cast<INT_PTR>(ShellExecuteA(nullptr,"open",url.c_str(),nullptr,nullptr,SW_SHOWNORMAL))<=32)diagnostic("browser launch");}
struct Context { std::string url; std::atomic<bool> *stopping; };
LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM w, LPARAM l) {
  auto *context = reinterpret_cast<Context *>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_CREATE) {
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams));
    return 0;
  }
  if (message == WM_APP + 1 && context) {
    if (l == WM_LBUTTONDBLCLK) openBrowser(context->url);
    if (l == WM_RBUTTONUP || l == WM_CONTEXTMENU) {
      auto menu = CreatePopupMenu();
      if(!menu){diagnostic("menu creation");return 0;}
      if(!AppendMenuW(menu,MF_STRING,1,L"Open Sight Reading") || !AppendMenuW(menu,MF_STRING,2,L"Quit and stop sound")){diagnostic("menu entries");DestroyMenu(menu);return 0;}
      POINT position{}; if(!GetCursorPos(&position)){diagnostic("cursor position");DestroyMenu(menu);return 0;}
      if(!SetForegroundWindow(window))diagnostic("menu foreground");
      auto choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, position.x, position.y, 0, window, nullptr);
      if(!DestroyMenu(menu))diagnostic("menu removal");
      if (choice == 1) openBrowser(context->url);
      if (choice == 2) *context->stopping = true;
    }
    return 0;
  }
  if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
  return DefWindowProcW(window, message, w, l);
}
}
void runTray(const std::string &url, std::atomic<bool> &stopping) {
  Context context{url, &stopping};
  WNDCLASSW klass{}; klass.lpfnWndProc = procedure; klass.hInstance = GetModuleHandleW(nullptr); klass.lpszClassName = L"SightReadingTray";
  auto registration=RegisterClassW(&klass);
  if(!registration && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){diagnostic("registration");return;}
  auto window = CreateWindowExW(0, klass.lpszClassName, L"Sight Reading", 0, 0, 0, 0, 0, nullptr, nullptr, klass.hInstance, &context);
  if (!window) {diagnostic("window creation");if(registration)UnregisterClassW(klass.lpszClassName,klass.hInstance);return;}
  NOTIFYICONDATAW icon{}; icon.cbSize = sizeof(icon); icon.hWnd = window; icon.uID = 1; icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; icon.uCallbackMessage = WM_APP + 1; icon.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
  lstrcpyW(icon.szTip, L"Sight Reading — MIDI and metronome remain active");
  const bool added=icon.hIcon && Shell_NotifyIconW(NIM_ADD, &icon);
  if(!added)diagnostic("icon creation");
  // Bounded native wait checks shutdown even when Windows never delivers a timer/message.
  while(added && !stopping.load()){
    auto result=MsgWaitForMultipleObjectsEx(0,nullptr,250,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
    if(result==WAIT_FAILED){diagnostic("message wait");break;}
    MSG message{};bool quit=false;
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT){quit=true;break;}TranslateMessage(&message);DispatchMessageW(&message);}
    if(quit)break;
  }
  if(added && !Shell_NotifyIconW(NIM_DELETE,&icon))diagnostic("icon removal");
  if(IsWindow(window) && !DestroyWindow(window))diagnostic("window removal");
  if(registration && !UnregisterClassW(klass.lpszClassName,klass.hInstance))diagnostic("class removal");
}
}
