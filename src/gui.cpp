#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
static HWND window_,output_,url_,encoder_,mode_,channel_,state_;
static PROCESS_INFORMATION process_{};
static HANDLE input_{};
static std::wstring value(HWND w) {int n=GetWindowTextLengthW(w);std::wstring s(n+1,L'\0');GetWindowTextW(w,s.data(),n+1);s.resize(n);return s;}
static HWND add(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id=0){
 return CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window_,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
}
static void status(const wchar_t* text){SetWindowTextW(state_,text);}
static void refresh(){
 if(process_.hProcess && WaitForSingleObject(process_.hProcess,0)==WAIT_OBJECT_0){
  CloseHandle(process_.hProcess);CloseHandle(process_.hThread);process_={};
  if(input_){CloseHandle(input_);input_=nullptr;}
  status(L"Stopped. Check the recording file.");
 }
 EnableWindow(GetDlgItem(window_,1),!process_.hProcess);
 EnableWindow(GetDlgItem(window_,2),process_.hProcess!=nullptr);
}
static void stop(){
 if(!input_)return;
 DWORD n=0;WriteFile(input_,"q\n",2,&n,nullptr);
 status(L"Stopping gracefully; finalizing MP4...");
}
static void start(){
 if(process_.hProcess)return;
 bool streaming=SendMessageW(mode_,CB_GETCURSEL,0,0)==1;
 bool cpu=SendMessageW(encoder_,CB_GETCURSEL,0,0)==1;
 std::wstring target=value(streaming?url_:output_);
 if(target.empty()||target.find(L'"')!=std::wstring::npos||target[0]==L'-'){
  MessageBoxW(window_,L"Invalid destination or stream URL.",L"LiteSR",MB_ICONERROR);return;
 }
 if(streaming && target.rfind(L"rtmp://",0)!=0 && target.rfind(L"rtmps://",0)!=0){
  MessageBoxW(window_,L"Stream URL must start with rtmp:// or rtmps://",L"LiteSR",MB_ICONERROR);return;
 }
 std::wstring cmd=L"ffmpeg.exe -hide_banner -loglevel error -f gdigrab -framerate 30 -i desktop -an -vf scale=1280:720,format=nv12 ";
 cmd+=cpu?L"-c:v libx264 -preset ultrafast ":L"-c:v h264_qsv -preset veryfast ";
 cmd+=L"-b:v 3000k -g 60 ";
 cmd+=streaming?L"-f flv ":L"-y -movflags +faststart ";
 cmd+=L"\""+target+L"\"";
 SECURITY_ATTRIBUTES sec={sizeof(sec),nullptr,TRUE};
 HANDLE read=nullptr,write=nullptr;
 if(!CreatePipe(&read,&write,&sec,0)){status(L"Cannot create pipe.");return;}
 SetHandleInformation(write,HANDLE_FLAG_INHERIT,0);
 STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESTDHANDLES;
 startup.hStdInput=read;startup.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);startup.hStdError=GetStdHandle(STD_ERROR_HANDLE);
 std::vector<wchar_t> args(cmd.begin(),cmd.end());args.push_back(0);
 BOOL ok=CreateProcessW(nullptr,args.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process_);
 CloseHandle(read);
 if(!ok){CloseHandle(write);process_={};status(L"Cannot start FFmpeg. Install it and add it to PATH.");return;}
 input_=write;status(streaming?L"Streaming...":L"Recording...");refresh();
}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){
 case WM_CREATE:
  window_=h;
  add(L"STATIC",L"LiteSR - experimental 720p / 30 FPS recorder",0,22,15,420,26);
  add(L"STATIC",L"Mode",0,22,60,90,20);
  mode_=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST,120,55,310,120);
  SendMessageW(mode_,CB_ADDSTRING,0,(LPARAM)L"Record MP4");
  SendMessageW(mode_,CB_ADDSTRING,0,(LPARAM)L"Stream RTMP / RTMPS");
  SendMessageW(mode_,CB_SETCURSEL,0,0);
  add(L"STATIC",L"Encoder",0,22,104,90,20);
  encoder_=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST,120,99,310,120);
  SendMessageW(encoder_,CB_ADDSTRING,0,(LPARAM)L"Intel QSV");
  SendMessageW(encoder_,CB_ADDSTRING,0,(LPARAM)L"CPU fallback");
  SendMessageW(encoder_,CB_SETCURSEL,0,0);
  add(L"STATIC",L"Output MP4",0,22,150,90,20);
  output_=add(L"EDIT",L"recording.mp4",WS_BORDER|ES_AUTOHSCROLL,120,145,310,26);
  add(L"STATIC",L"Stream URL",0,22,196,90,20);
  url_=add(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,120,191,310,26);
  add(L"BUTTON",L"Start",BS_PUSHBUTTON,22,248,198,36,1);
  add(L"BUTTON",L"Stop",BS_PUSHBUTTON,231,248,198,36,2);
  state_=add(L"STATIC",L"Ready. Requires ffmpeg.exe on PATH.",0,22,302,415,35);
  add(L"STATIC",L"Updates",0,22,353,90,20);
  channel_=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST,120,348,180,120);
  SendMessageW(channel_,CB_ADDSTRING,0,(LPARAM)L"Stable releases");
  SendMessageW(channel_,CB_ADDSTRING,0,(LPARAM)L"Beta / Alpha releases");
  SendMessageW(channel_,CB_SETCURSEL,1,0);
  add(L"BUTTON",L"Check releases",BS_PUSHBUTTON,312,348,117,28,3);
  add(L"STATIC",L"Alpha: no audio or in-game capture yet.",0,22,401,405,24);
  SetTimer(h,1,500,nullptr);refresh();return 0;
 case WM_COMMAND:
  if(LOWORD(w)==1)start();
  if(LOWORD(w)==2)stop();
  if(LOWORD(w)==3)ShellExecuteW(h,L"open",
   SendMessageW(channel_,CB_GETCURSEL,0,0)==0?
   L"https://github.com/awnkr-par/litesr/releases/latest":
   L"https://github.com/awnkr-par/litesr/releases",nullptr,nullptr,SW_SHOWNORMAL);
  return 0;
 case WM_TIMER:refresh();return 0;
 case WM_CLOSE:
  if(process_.hProcess){stop();MessageBoxW(h,L"Finish stopping FFmpeg before exiting.",L"LiteSR",MB_OK);return 0;}
  DestroyWindow(h);return 0;
 case WM_DESTROY:PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
 WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=proc;wc.lpszClassName=L"LiteSRWindow";
 wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
 RegisterClassW(&wc);
 HWND h=CreateWindowExW(0,L"LiteSRWindow",L"LiteSR 0.2 Alpha",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
 CW_USEDEFAULT,CW_USEDEFAULT,467,483,nullptr,nullptr,instance,nullptr);
 ShowWindow(h,show);MSG msg;
 while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 return 0;
}
