#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cwchar>
static HWND sourceMode{},sourceChoice{};
struct Capture{std::wstring label;RECT bounds{};HWND window{};bool display{};};
static std::vector<Capture> captures;
static HWND root{}, mode{}, service{}, server{}, key{}, fileName{}, resolution{}, fps{}, bitrate{}, encoder{}, audio{}, updates{}, state{}, startButton{}, stopButton{};
static PROCESS_INFORMATION process{};
static HANDLE stdinPipe{};
static const wchar_t* buildVersion=L"0.2.0-alpha.7";
static std::wstring get(HWND h){int n=GetWindowTextLengthW(h);std::wstring s(n+1,L'\0');GetWindowTextW(h,s.data(),n+1);s.resize(n);return s;}
static HWND add(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id=0){
 HWND v=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,root,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
 SendMessageW(v,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);return v;
}
static HWND combo(int x,int y,int w,const std::vector<const wchar_t*>& options,int selected=0){
 HWND h=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,x,y,w,150);
 for(auto v:options)SendMessageW(h,CB_ADDSTRING,0,(LPARAM)v);
 SendMessageW(h,CB_SETCURSEL,selected,0);return h;
}
static int selection(HWND h){return (int)SendMessageW(h,CB_GETCURSEL,0,0);}
static void select(HWND h,int n){SendMessageW(h,CB_SETCURSEL,n,0);}
static std::wstring directory(){
 wchar_t file[MAX_PATH]{};GetModuleFileNameW(nullptr,file,MAX_PATH);std::wstring s(file);
 return s.substr(0,s.find_last_of(L"\\/")+1);
}
static std::wstring cfg(){
 wchar_t path[MAX_PATH]{};DWORD n=GetEnvironmentVariableW(L"APPDATA",path,MAX_PATH);
 if(!n||n>=MAX_PATH)return directory()+L"LiteSR.ini";
 std::wstring dir=std::wstring(path)+L"\\LiteSR";
 CreateDirectoryW(dir.c_str(),nullptr);return dir+L"\\settings.ini";
}
static void save(){
 auto p=cfg();
 auto put=[&](const wchar_t* name,const std::wstring& value){WritePrivateProfileStringW(L"LiteSR",name,value.c_str(),p.c_str());};
 put(L"mode",std::to_wstring(selection(mode)));put(L"service",std::to_wstring(selection(service)));
 put(L"server",get(server));put(L"output",get(fileName));
 put(L"resolution",std::to_wstring(selection(resolution)));put(L"fps",std::to_wstring(selection(fps)));
 put(L"bitrate",get(bitrate));put(L"encoder",std::to_wstring(selection(encoder)));
 put(L"audio",std::to_wstring(selection(audio)));put(L"channel",std::to_wstring(selection(updates)));
 // Stream keys are deliberately never written to disk.
}
static std::wstring load(const wchar_t* name,const wchar_t* fallback){
 wchar_t buffer[1024]{};GetPrivateProfileStringW(L"LiteSR",name,fallback,buffer,1024,cfg().c_str());return buffer;
}
static int loadIndex(const wchar_t* name,int fallback,int maximum){
 try {int n=std::stoi(load(name,std::to_wstring(fallback).c_str()));return n>=0&&n<maximum?n:fallback;}
 catch(...){return fallback;}
}
static void setStatus(const wchar_t* s){SetWindowTextW(state,s);}
static void refresh(){
 if(process.hProcess&&WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0){
  DWORD exitCode=0;GetExitCodeProcess(process.hProcess,&exitCode);
  CloseHandle(process.hThread);CloseHandle(process.hProcess);process={};
  if(stdinPipe){CloseHandle(stdinPipe);stdinPipe=nullptr;}
  setStatus(exitCode==0?L"Stopped normally.":L"FFmpeg stopped with an error. Check encoder or stream settings.");
 }
 bool busy=process.hProcess!=nullptr;
 EnableWindow(startButton,!busy);EnableWindow(stopButton,busy);
}
static void stop(){
 if(stdinPipe){DWORD written=0;WriteFile(stdinPipe,"q\n",2,&written,nullptr);setStatus(L"Finishing recording/stream...");}
}
static std::wstring quote(const std::wstring& input){
 std::wstring out=L"\"";int slashes=0;
 for(wchar_t c:input){
  if(c==L'\\'){slashes++;continue;}
  if(c==L'"'){out.append(slashes*2+1,L'\\');out+=c;slashes=0;continue;}
  out.append(slashes,L'\\');slashes=0;out+=c;
 }
 out.append(slashes*2,L'\\');return out+L"\"";
}
static BOOL CALLBACK findDisplay(HMONITOR mon,HDC,LPRECT,LPARAM){
 MONITORINFOEXW mi{};mi.cbSize=sizeof(mi);if(!GetMonitorInfoW(mon,&mi))return TRUE;
 auto r=mi.rcMonitor;std::wstring label=mi.szDevice;
 label+=L" ("+std::to_wstring(r.right-r.left)+L"x"+std::to_wstring(r.bottom-r.top)+L")";
 captures.push_back({label,r,nullptr,true});return TRUE;
}
static BOOL CALLBACK findWindow(HWND win,LPARAM){
 if(!IsWindowVisible(win)||IsIconic(win)||win==root)return TRUE;
 wchar_t name[256]{};GetWindowTextW(win,name,256);
 RECT rect{};if(!name[0]||!GetWindowRect(win,&rect)||rect.right-rect.left<80)return TRUE;
 captures.push_back({name,rect,win,false});return TRUE;
}
static void listCapture(){
 captures.clear();SendMessageW(sourceChoice,CB_RESETCONTENT,0,0);
 if(selection(sourceMode)==0)EnumDisplayMonitors(nullptr,nullptr,findDisplay,0);
 else EnumWindows(findWindow,0);
 for(const auto& c:captures)SendMessageW(sourceChoice,CB_ADDSTRING,0,(LPARAM)c.label.c_str());
 if(!captures.empty())select(sourceChoice,0);
}
static void serviceChanged(){
 int n=selection(service);
 if(n==0)SetWindowTextW(server,L"rtmps://a.rtmp.youtube.com/live2");
 if(n==1)SetWindowTextW(server,L"rtmp://live.twitch.tv/app");
 if(n==2)SetWindowTextW(server,L"");
}
static bool validNumber(const std::wstring& s,int min,int max,int& n){
 try{size_t p=0;int v=std::stoi(s,&p);if(p!=s.size()||v<min||v>max)return false;n=v;return true;}catch(...){return false;}
}
static void start(){
 if(process.hProcess)return;
 bool isStreaming=selection(mode)==1;
 int rate=3000;
 if(!validNumber(get(bitrate),300,50000,rate)){MessageBoxW(root,L"Bitrate must be from 300 to 50000 Kbps.",L"LiteSR",MB_ICONWARNING);return;}
 const int widths[]={854,1280,1920};const int heights[]={480,720,1080};
 int r=std::clamp(selection(resolution),0,2);
 int fr=selection(fps)==0?30:60;
 std::wstring dest;
 if(isStreaming){
  std::wstring base=get(server),secret=get(key);
  if(base.rfind(L"rtmp://",0)!=0&&base.rfind(L"rtmps://",0)!=0){MessageBoxW(root,L"Enter a valid RTMP/RTMPS server URL.",L"LiteSR",MB_ICONWARNING);return;}
  if(secret.empty()){MessageBoxW(root,L"Enter your stream key. It will not be saved.",L"LiteSR",MB_ICONWARNING);return;}
  while(!base.empty()&&base.back()==L'/')base.pop_back();
  while(!secret.empty()&&secret.front()==L'/')secret.erase(secret.begin());
  dest=base+L"/"+secret;
 }else{
  dest=get(fileName);
  if(dest.empty()){MessageBoxW(root,L"Enter a recording filename.",L"LiteSR",MB_ICONWARNING);return;}
 }
 if(dest.find(L'"')!=std::wstring::npos||dest.find(L'\n')!=std::wstring::npos){
  MessageBoxW(root,L"Invalid quotes or newline in destination.",L"LiteSR",MB_ICONWARNING);return;
 }
 std::wstring ff=directory()+L"ffmpeg\\bin\\ffmpeg.exe";
 if(GetFileAttributesW(ff.c_str())==INVALID_FILE_ATTRIBUTES){setStatus(L"FFmpeg not found. Reinstall LiteSR.");return;}
 if(selection(sourceChoice)<0||selection(sourceChoice)>=(int)captures.size()){
  MessageBoxW(root,L"Choose a capture source.",L"LiteSR",MB_OK);return;
 }
 auto selected=captures[(size_t)selection(sourceChoice)];
 std::vector<std::wstring> args={ff,L"-hide_banner",L"-loglevel",L"error",L"-f",L"gdigrab",L"-framerate",std::to_wstring(fr)};
 if(selected.display){
  args.insert(args.end(),{L"-offset_x",std::to_wstring(selected.bounds.left),L"-offset_y",std::to_wstring(selected.bounds.top),
  L"-video_size",std::to_wstring(selected.bounds.right-selected.bounds.left)+L"x"+std::to_wstring(selected.bounds.bottom-selected.bounds.top),L"-i",L"desktop"});
 }else{
  wchar_t handle[40]{};swprintf_s(handle,40,L"hwnd=0x%llx",(unsigned long long)(uintptr_t)selected.window);
  args.insert(args.end(),{L"-i",handle});
 }
 // Video capture is operational. Audio capture is not advertised until device enumeration is implemented.
 args.insert(args.end(),{L"-an",L"-vf",L"scale="+std::to_wstring(widths[r])+L":"+std::to_wstring(heights[r])+L",format=nv12"});
 if(selection(encoder)==0)args.insert(args.end(),{L"-c:v",L"h264_qsv",L"-preset",L"veryfast"});
 else args.insert(args.end(),{L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-tune",L"zerolatency"});
 args.insert(args.end(),{L"-b:v",std::to_wstring(rate)+L"k",L"-maxrate",std::to_wstring(rate)+L"k",
  L"-bufsize",std::to_wstring(rate*2)+L"k",L"-g",std::to_wstring(fr*2)});
 if(isStreaming)args.insert(args.end(),{L"-f",L"flv",dest});
 else args.insert(args.end(),{L"-y",L"-movflags",L"+faststart",dest});
 std::wstring cmd;
 for(const auto& a:args){if(!cmd.empty())cmd+=L" ";cmd+=quote(a);}
 SECURITY_ATTRIBUTES sa={sizeof(sa),nullptr,TRUE};HANDLE read{},write{};
 if(!CreatePipe(&read,&write,&sa,0)){setStatus(L"Could not create FFmpeg stdin.");return;}
 SetHandleInformation(write,HANDLE_FLAG_INHERIT,0);
 STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=read;
 si.hStdOutput=GetStdHandle(STD_OUTPUT_HANDLE);si.hStdError=GetStdHandle(STD_ERROR_HANDLE);
 std::vector<wchar_t> buffer(cmd.begin(),cmd.end());buffer.push_back(0);
 BOOL ok=CreateProcessW(ff.c_str(),buffer.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&process);
 CloseHandle(read);
 if(!ok){CloseHandle(write);process={};setStatus(L"Failed launching FFmpeg.");return;}
 stdinPipe=write;save();setStatus(isStreaming?L"Stream launched. Confirm live signal on platform.":L"Recording started.");refresh();
}
static void checkUpdates(bool manual){
 std::wstring script=directory()+L"updater\\update.ps1";
 if(GetFileAttributesW(script.c_str())==INVALID_FILE_ATTRIBUTES)return;
 std::wstring command=L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File "+
 quote(script)+L" -Channel "+(selection(updates)==0?L"stable":L"beta")+L" -ParentPid "+std::to_wstring(GetCurrentProcessId());
 if(manual)command+=L" -Manual";
 STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 std::vector<wchar_t> buffer(command.begin(),command.end());buffer.push_back(0);
 if(CreateProcessW(nullptr,buffer.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){
  CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 }
}
static LRESULT CALLBACK wnd(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){
 case WM_CREATE:{
  root=h;
  add(L"STATIC",L"LiteSR  |  Record + Stream  |  Experimental",0,18,13,520,22);
  add(L"STATIC",L"Mode",0,20,52,104,22);
  mode=combo(132,47,400,{L"Record to MP4",L"Live stream"});
  add(L"STATIC",L"Streaming service",0,20,93,110,22);
  service=combo(132,89,400,{L"YouTube",L"Twitch",L"Custom"});
  add(L"STATIC",L"Server URL",0,20,133,105,22);
  server=add(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,132,128,400,27);
  add(L"STATIC",L"Stream key",0,20,173,100,22);
  key=add(L"EDIT",L"",WS_BORDER|ES_PASSWORD|ES_AUTOHSCROLL,132,168,400,27);
  add(L"STATIC",L"MP4 filename",0,20,213,105,22);
  fileName=add(L"EDIT",L"recording.mp4",WS_BORDER|ES_AUTOHSCROLL,132,208,400,27);
  add(L"STATIC",L"Capture source",0,20,256,110,22);
  sourceMode=combo(132,251,180,{L"Monitor",L"App window"});
  add(L"BUTTON",L"Refresh",BS_PUSHBUTTON,425,251,107,28,4);
  sourceChoice=combo(132,289,400,{});
  add(L"STATIC",L"Output quality",0,20,345,110,22);
  resolution=combo(132,340,173,{L"480p (854x480)",L"720p (1280x720)",L"1080p (1920x1080)"},1);
  fps=combo(320,340,95,{L"30 FPS",L"60 FPS"});
  add(L"STATIC",L"Bitrate Kbps",0,20,388,110,22);
  bitrate=add(L"EDIT",L"3000",WS_BORDER|ES_NUMBER,132,383,120,27);
  add(L"STATIC",L"Encoder",0,270,388,70,22);
  encoder=combo(340,383,192,{L"Intel QSV",L"CPU x264"});
  add(L"STATIC",L"Audio",0,20,431,110,22);
  audio=combo(132,426,400,{L"No audio (device capture coming soon)"});
  startButton=add(L"BUTTON",L"Start",BS_PUSHBUTTON|WS_TABSTOP,20,482,248,42,1);
  stopButton=add(L"BUTTON",L"Stop",BS_PUSHBUTTON|WS_TABSTOP,283,482,249,42,2);
  state=add(L"STATIC",L"Ready. FFmpeg included.",0,20,535,520,45);
  add(L"STATIC",L"Updates",0,20,600,90,22);
  updates=combo(132,595,200,{L"Stable",L"Beta / Alpha"},1);
  add(L"BUTTON",L"Check updates",BS_PUSHBUTTON,350,595,182,28,3);
  add(L"STATIC",L"Do not share stream keys. GPU performance depends on hardware/driver.",0,20,639,525,25);
  select(mode,loadIndex(L"mode",0,2));select(service,loadIndex(L"service",0,3));
  serviceChanged();
  auto savedServer=load(L"server",L"");if(!savedServer.empty())SetWindowTextW(server,savedServer.c_str());
  SetWindowTextW(fileName,load(L"output",L"recording.mp4").c_str());
  select(resolution,loadIndex(L"resolution",1,3));select(fps,loadIndex(L"fps",0,2));
  SetWindowTextW(bitrate,load(L"bitrate",L"3000").c_str());
  select(encoder,loadIndex(L"encoder",0,2));select(updates,loadIndex(L"channel",1,2));
  listCapture();SetTimer(h,1,500,nullptr);SetTimer(h,2,3500,nullptr);refresh();return 0;
 }
 case WM_COMMAND:
  if(LOWORD(w)==1)start();
  if(LOWORD(w)==2)stop();
  if(LOWORD(w)==3)checkUpdates(true);
  if(LOWORD(w)==4||((HWND)l==sourceMode&&HIWORD(w)==CBN_SELCHANGE))listCapture();
  if((HWND)l==service&&HIWORD(w)==CBN_SELCHANGE)serviceChanged();
  return 0;
 case WM_TIMER:
  if(w==2){KillTimer(h,2);checkUpdates(false);}else refresh();return 0;
 case WM_CLOSE:
  if(process.hProcess){stop();MessageBoxW(h,L"Stop and finalize recording before closing.",L"LiteSR",MB_OK);return 0;}
  save();DestroyWindow(h);return 0;
 case WM_DESTROY:PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
 WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=wnd;wc.lpszClassName=L"LiteSRMain";
 wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
 RegisterClassW(&wc);
 HWND h=CreateWindowExW(0,L"LiteSRMain",L"LiteSR v0.2.0-alpha.7",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
 CW_USEDEFAULT,CW_USEDEFAULT,562,725,nullptr,nullptr,instance,nullptr);
 ShowWindow(h,show);MSG msg;
 while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 return 0;
}
