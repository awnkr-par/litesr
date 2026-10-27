#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <shlobj.h>
#pragma comment(lib,"comdlg32.lib")
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <fstream>
#include <sstream>
#include <uxtheme.h>
#include <mmsystem.h>
#pragma comment(lib,"winmm.lib")
#pragma comment(lib,"uxtheme.lib")
static HBRUSH darkBrush{},fieldBrush{};
static bool darkMode=true;
static HWND themeButton{};
static RECT previewArea={26,94,902,558};
static const RECT basePreview={26,94,902,558};
static bool fullscreen=false;
static DWORD savedStyle=0,savedExStyle=0;
static WINDOWPLACEMENT savedPlacement{sizeof(WINDOWPLACEMENT)};
static HWND fullscreenButton{};
struct UiPosition{HWND h;RECT bounds;};
static std::vector<UiPosition> uiPositions;
static bool layoutReady=false;
static HWND settingsWindow{},settingsServer{},settingsResolution{},settingsFps{},settingsBitrate{},settingsEncoder{},settingsBackend{},settingsContrast{},settingsLargeText{},settingsReducedPreview{};
static bool highContrast=false,largeText=false,reducedPreview=false,reduceMotion=true;
static bool gentleAnimations=false;
static HWND settingsAnimations{};
static int navigationStep=0;
static bool navigationAnimating=false;
static HWND settingsReduceMotion{};

static HFONT largeFont{};
static void showSettings();
static std::vector<HWND> studioPage,settingsPage;
static bool settingsVisible=false;
static HWND pageTitle{},settingsBack{};
static HFONT uiFont{};
static COLORREF accent=RGB(85,135,250);
static void applyTheme();
static COLORREF ink=RGB(230,235,246);
static COLORREF canvas=RGB(17,22,34);
static COLORREF fieldColor=RGB(30,38,54);
static COLORREF panelColor=RGB(24,33,51);
static HWND sourceMode{},sourceChoice{};
struct Capture{std::wstring label;RECT bounds{};HWND window{};bool display{};};
static std::vector<Capture> captures;
static HWND root{}, mode{}, service{}, server{}, key{}, fileName{}, resolution{}, fps{}, bitrate{}, encoder{}, audio{}, updates{}, state{}, startButton{}, stopButton{};
static PROCESS_INFORMATION process{};
static HANDLE stdinPipe{};
static HANDLE logHandle{};
static std::wstring logPath;
static bool updateExitRequested=false;
static bool captureStreaming=false,previewEnabled=true;
static std::wstring activeOutput;
static HWND diskInfo{},browseButton{},openFolderButton{},qualityPreset{};
static std::vector<std::wstring> microphoneDevices;
static HWND audioRefreshButton{};
static bool stopRequested=false;
static ULONGLONG stopRequestedAt=0;
static std::wstring sessionStartedAt;
static std::wstring sessionBackend,sessionEncoder,sessionSource;
static int sessionResolution=0,sessionTargetFPS=0;
static HWND historyButton{};
static HWND captureBackend{},captureHealth{};
static std::wstring progressPath;
static int requestedFPS=30;
static bool usingDDA=false;
static ULONGLONG captureStarted=0;
static HWND previewButton{}, sessionInfo{};
static std::wstring selectedCaptureLabel;
static int selectedCaptureMode=0;
static const wchar_t* buildVersion=L"0.2.0-alpha.26";
static void layoutWorkspace(){
 if(!root||!layoutReady)return;
 RECT client{};GetClientRect(root,&client);
 double x=(double)(client.right-client.left)/1244.0;
 double y=(double)(client.bottom-client.top)/886.0;
 HDWP batch=BeginDeferWindowPos((int)uiPositions.size());
 if(!batch)return;
 for(const auto& entry:uiPositions){
  const RECT& b=entry.bounds;
  int left=(int)(b.left*x),top=(int)(b.top*y);

  int width=std::max(1,(int)((b.right-b.left)*x));
  int height=std::max(1,(int)((b.bottom-b.top)*y));
  batch=DeferWindowPos(batch,entry.h,nullptr,left,top,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
  if(!batch)break;
 }
 if(batch)EndDeferWindowPos(batch);
 previewArea={(LONG)(basePreview.left*x),(LONG)(basePreview.top*y),(LONG)(basePreview.right*x),(LONG)(basePreview.bottom*y)};
 InvalidateRect(root,nullptr,TRUE);
}
static void toggleFullscreen(){
 if(!root)return;
 if(!fullscreen){
  savedStyle=(DWORD)GetWindowLongPtrW(root,GWL_STYLE);
  savedExStyle=(DWORD)GetWindowLongPtrW(root,GWL_EXSTYLE);
  savedPlacement.length=sizeof(savedPlacement);
  GetWindowPlacement(root,&savedPlacement);
  HMONITOR monitor=MonitorFromWindow(root,MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi{sizeof(mi)};if(!GetMonitorInfoW(monitor,&mi))return;
  fullscreen=true;
  SetWindowLongPtrW(root,GWL_STYLE,(LONG_PTR)(savedStyle&~(WS_CAPTION|WS_THICKFRAME|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SYSMENU)));
  SetWindowLongPtrW(root,GWL_EXSTYLE,(LONG_PTR)(savedExStyle&~(WS_EX_DLGMODALFRAME|WS_EX_CLIENTEDGE|WS_EX_STATICEDGE)));
  SetWindowPos(root,HWND_TOP,mi.rcMonitor.left,mi.rcMonitor.top,mi.rcMonitor.right-mi.rcMonitor.left,mi.rcMonitor.bottom-mi.rcMonitor.top,SWP_FRAMECHANGED|SWP_SHOWWINDOW);
 }else{
  fullscreen=false;
  SetWindowLongPtrW(root,GWL_STYLE,(LONG_PTR)savedStyle);
  SetWindowLongPtrW(root,GWL_EXSTYLE,(LONG_PTR)savedExStyle);
  SetWindowPlacement(root,&savedPlacement);
  SetWindowPos(root,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
 }
 if(fullscreenButton)SetWindowTextW(fullscreenButton,fullscreen?L"Exit fullscreen [F11]":L"Fullscreen [F11]");
 layoutWorkspace();
}
static std::wstring get(HWND h){int n=GetWindowTextLengthW(h);std::wstring s(n+1,L'\0');GetWindowTextW(h,s.data(),n+1);s.resize(n);return s;}
static HWND add(const wchar_t* cls,const wchar_t* text,DWORD style,int x,int y,int w,int h,int id=0){
 if(wcscmp(cls,L"STATIC")==0)style|=SS_OWNERDRAW;
 if(wcscmp(cls,L"BUTTON")==0 && (style&BS_TYPEMASK)!=BS_GROUPBOX && (style&BS_TYPEMASK)!=BS_AUTOCHECKBOX)style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;
 HWND v=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,root,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
 if(v)uiPositions.push_back({v,{x,y,x+w,y+h}});
 SendMessageW(v,WM_SETFONT,(WPARAM)(uiFont?uiFont:GetStockObject(DEFAULT_GUI_FONT)),TRUE);
 if(wcscmp(cls,L"COMBOBOX")==0||wcscmp(cls,L"EDIT")==0||wcscmp(cls,L"BUTTON")==0)SetWindowTheme(v,darkMode?L"DarkMode_Explorer":L"Explorer",nullptr);
 return v;
}
static HWND combo(int x,int y,int w,const std::vector<const wchar_t*>& options,int selected=0){
 HWND h=add(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,x,y,w,180);
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
 put(L"theme",darkMode?L"dark":L"light");
 put(L"backend",std::to_wstring(selection(captureBackend)));
 put(L"reduceMotion",L"1");put(L"gentleAnimations",gentleAnimations?L"1":L"0");
 put(L"highContrast",highContrast?L"1":L"0");put(L"largeText",largeText?L"1":L"0");put(L"reducedPreview",reducedPreview?L"1":L"0");
 put(L"sourceMode",std::to_wstring(selection(sourceMode)));
 put(L"sourceName",selectedCaptureLabel);
 put(L"preview",previewEnabled?L"1":L"0");
 put(L"microphone",selection(audio)>0&&selection(audio)<=(int)microphoneDevices.size()?microphoneDevices[(size_t)selection(audio)-1]:L"");
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
static void applyTheme(){
 canvas=highContrast?(darkMode?RGB(0,0,0):RGB(255,255,255)):(darkMode?RGB(17,22,34):RGB(244,247,253));
 panelColor=darkMode?RGB(24,33,51):RGB(229,237,250);
 fieldColor=darkMode?RGB(30,38,54):RGB(255,255,255);
 ink=highContrast?(darkMode?RGB(255,255,255):RGB(0,0,0)):(darkMode?RGB(230,235,246):RGB(31,45,73));
 if(darkBrush)DeleteObject(darkBrush);
 if(fieldBrush)DeleteObject(fieldBrush);
 darkBrush=CreateSolidBrush(canvas);fieldBrush=CreateSolidBrush(fieldColor);
 if(root){
  SetWindowTextW(themeButton,darkMode?L"Light theme":L"Dark theme");
  EnumChildWindows(root,[](HWND h,LPARAM mode)->BOOL{
   wchar_t cls[32]{};GetClassNameW(h,cls,32);
   if(wcscmp(cls,L"BUTTON")==0||wcscmp(cls,L"COMBOBOX")==0||wcscmp(cls,L"EDIT")==0)
    SetWindowTheme(h,mode?L"DarkMode_Explorer":L"Explorer",nullptr);
   SendMessageW(h,WM_SETFONT,(WPARAM)(largeText&&largeFont?largeFont:uiFont),TRUE);
   InvalidateRect(h,nullptr,TRUE);return TRUE;
  },(LPARAM)(darkMode?1:0));
  InvalidateRect(root,nullptr,TRUE);
 }
}
static std::wstring recordingsFolder(){
 // Create C:\\LiteSR when permissions allow. Do not require elevation.
 const wchar_t* preferred=L"C:\\LiteSR";
 if(CreateDirectoryW(preferred,nullptr)||GetLastError()==ERROR_ALREADY_EXISTS){
  DWORD attr=GetFileAttributesW(preferred);
  if(attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_DIRECTORY))return preferred;
 }
 wchar_t path[MAX_PATH]{};if(SUCCEEDED(SHGetFolderPathW(nullptr,CSIDL_MYVIDEO,nullptr,SHGFP_TYPE_CURRENT,path)))return std::wstring(path);
 return directory();
}
static void openRecordingFolder(){
 std::wstring file=get(fileName);std::wstring folder=recordingsFolder();
 size_t slash=file.find_last_of(L"\\/");if(slash!=std::wstring::npos)folder=file.substr(0,slash);
 ShellExecuteW(root,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
}
static void browseRecording(){
 wchar_t path[MAX_PATH]{};std::wstring current=get(fileName);
 if(!current.empty()&&current.size()<MAX_PATH)wcscpy_s(path,MAX_PATH,current.c_str());
 OPENFILENAMEW d{};d.lStructSize=sizeof(d);d.hwndOwner=root;
 d.lpstrFilter=L"MP4 video (*.mp4)\0*.mp4\0All files (*.*)\0*.*\0\0";
 d.lpstrFile=path;d.nMaxFile=MAX_PATH;d.lpstrDefExt=L"mp4";
 d.Flags=OFN_PATHMUSTEXIST|OFN_OVERWRITEPROMPT;
 if(GetSaveFileNameW(&d))SetWindowTextW(fileName,path);
}
static void applyPreset(){
 int p=selection(qualityPreset);
 const int resolutions[]={0,1,2};const wchar_t* rates[]={L"1200",L"2500",L"4500"};
 if(p>=0&&p<3){select(resolution,resolutions[p]);select(fps,0);SetWindowTextW(bitrate,rates[p]);save();}
}
static void setStatus(const wchar_t* s){SetWindowTextW(state,s);}
static std::wstring sessionHistoryPath(){
 std::wstring path=cfg();
 return path.substr(0,path.find_last_of(L"\\/")+1)+L"sessions.tsv";
}
static std::wstring cleanedField(std::wstring value){
 for(wchar_t& c:value)if(c==L'\t'||c==L'\r'||c==L'\n')c=L' ';
 return value;
}
static std::wstring newRecordingName(const std::wstring& input){
 if(GetFileAttributesW(input.c_str())==INVALID_FILE_ATTRIBUTES)return input;
 size_t dot=input.find_last_of(L'.'),slash=input.find_last_of(L"\\/");
 if(dot==std::wstring::npos||(slash!=std::wstring::npos&&dot<slash))dot=input.size();
 for(int n=1;n<=9999;++n){
  std::wstring candidate=input.substr(0,dot)+L" ("+std::to_wstring(n)+L")"+input.substr(dot);
  if(GetFileAttributesW(candidate.c_str())==INVALID_FILE_ATTRIBUTES)return candidate;
 }
 return L"";
}
static void recordSession(DWORD code){
 if(sessionStartedAt.empty())return;
 std::wofstream out(sessionHistoryPath(),std::ios::app);
 if(out){
  out<<cleanedField(sessionStartedAt)<<L"\t"
     <<(captureStreaming?L"STREAM":L"RECORD")<<L"\t"
     <<(code==0?L"SUCCESS":L"FAILED")<<L"\t"
     <<code<<L"\t"<<cleanedField(sessionSource)<<L"\t"
     <<sessionResolution<<L"p\t"<<sessionTargetFPS<<L"fps\t"
     <<cleanedField(sessionBackend)<<L"\t"<<cleanedField(sessionEncoder)<<L"\t"
     <<(captureStreaming?L"(stream destination hidden)":cleanedField(activeOutput))<<L"\n";
 }
 sessionStartedAt.clear();
}
static bool diskReady(const std::wstring& output,int kbps){
 size_t slash=output.find_last_of(L"\\/");
 std::wstring folder=slash==std::wstring::npos?recordingsFolder():output.substr(0,slash);
 ULARGE_INTEGER available{};
 if(!GetDiskFreeSpaceExW(folder.c_str(),&available,nullptr,nullptr)){
  MessageBoxW(root,L"Recording folder is unavailable or not writable.",L"LiteSR",MB_ICONWARNING);return false;
 }
 // 512 MiB emergency headroom, plus ten minutes of expected video at configured bitrate.
 unsigned long long required=512ULL*1024*1024+(unsigned long long)kbps*1000*600/8;
 if(available.QuadPart<required){
  MessageBoxW(root,L"Not enough free space for a safe recording start. Free disk space or choose a different destination.",L"LiteSR",MB_ICONWARNING);return false;
 }
 return true;
}
static void readCaptureHealth(){
 if(!captureHealth||!process.hProcess||progressPath.empty())return;
 std::ifstream input(progressPath,std::ios::binary);
 if(!input)return;
 std::string line;std::string fpsText,dropText,dupText,frameText,speedText;
 while(std::getline(input,line)){
  if(line.rfind("fps=",0)==0)fpsText=line.substr(4);
  if(line.rfind("drop_frames=",0)==0)dropText=line.substr(12);
  if(line.rfind("dup_frames=",0)==0)dupText=line.substr(11);
  if(line.rfind("frame=",0)==0)frameText=line.substr(6);
  if(line.rfind("speed=",0)==0)speedText=line.substr(6);
 }
 auto trim=[](std::string a){size_t p=a.find_first_not_of(" \t\r");return p==std::string::npos?std::string():a.substr(p);};
 double fpsValue=0;try{fpsValue=std::stod(trim(fpsText));}catch(...){}
 std::wstring summary=L"ENCODE FPS: "+std::to_wstring((int)(fpsValue+0.5))+L" / "+std::to_wstring(requestedFPS);
 if(!dropText.empty())summary+=L"  |  DROPS: "+std::wstring(dropText.begin(),dropText.end());
 if(!dupText.empty())summary+=L"  |  DUP: "+std::wstring(dupText.begin(),dupText.end());
 if(!speedText.empty())summary+=L"  |  SPEED: "+std::wstring(speedText.begin(),speedText.end());
 SetWindowTextW(captureHealth,summary.c_str());
}
static void refresh(){
 if(process.hProcess&&WaitForSingleObject(process.hProcess,0)==WAIT_OBJECT_0){
  DWORD exitCode=0;GetExitCodeProcess(process.hProcess,&exitCode);
  CloseHandle(process.hThread);CloseHandle(process.hProcess);process={};
  if(stdinPipe){CloseHandle(stdinPipe);stdinPipe=nullptr;}
  if(logHandle){CloseHandle(logHandle);logHandle=nullptr;}
  recordSession(exitCode);stopRequested=false;stopRequestedAt=0;
  captureStarted=0;
  if(captureHealth)SetWindowTextW(captureHealth,L"Capture stopped - check log for results.");
  if(!captureStreaming && !activeOutput.empty() && exitCode==0){
   std::wstring line=L"Recording completed: "+activeOutput;
   setStatus(line.c_str());
  }else setStatus(exitCode==0?L"Stopped normally.":L"FFmpeg stopped with an error. Check encoder or stream settings.");
 }
 if(updateExitRequested&&!process.hProcess){save();DestroyWindow(root);return;}
 bool busy=process.hProcess!=nullptr;
 if(diskInfo){std::wstring p=get(fileName);std::wstring dir=recordingsFolder();size_t slash=p.find_last_of(L"\\/");if(slash!=std::wstring::npos)dir=p.substr(0,slash);
  ULARGE_INTEGER freeBytes{};if(GetDiskFreeSpaceExW(dir.c_str(),&freeBytes,nullptr,nullptr)){
   std::wstring label=L"Free disk: "+std::to_wstring(freeBytes.QuadPart/(1024ULL*1024*1024))+L" GB";SetWindowTextW(diskInfo,label.c_str());
  }
 }
 if(busy)readCaptureHealth();
 if(busy&&captureStarted){ULONGLONG sec=(GetTickCount64()-captureStarted)/1000;
  std::wstring info=std::wstring(captureStreaming?L"LIVE  |  ":L"RECORDING  |  ")+std::to_wstring(sec/3600)+L"h "+std::to_wstring((sec/60)%60)+L"m "+std::to_wstring(sec%60)+L"s  |  F10 to stop";
  if(sessionInfo)SetWindowTextW(sessionInfo,info.c_str());
 }else if(sessionInfo)SetWindowTextW(sessionInfo,L"IDLE  |  F9 start  /  F10 stop");
 EnableWindow(startButton,!busy);EnableWindow(stopButton,busy&&!stopRequested);
}
static void stop(){
 if(stopRequested||!stdinPipe)return;
 stopRequested=true;stopRequestedAt=GetTickCount64();
 DWORD written=0;WriteFile(stdinPipe,"q\n",2,&written,nullptr);
 setStatus(L"Finishing recording/stream... Please wait.");
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
 std::wstring prev=selectedCaptureLabel;
 if(sourceChoice){int i=selection(sourceChoice);if(i>=0&&i<(int)captures.size())prev=captures[(size_t)i].label;}
 captures.clear();SendMessageW(sourceChoice,CB_RESETCONTENT,0,0);
 if(selection(sourceMode)==0)EnumDisplayMonitors(nullptr,nullptr,findDisplay,0);
 else EnumWindows(findWindow,0);
 for(const auto& c:captures)SendMessageW(sourceChoice,CB_ADDSTRING,0,(LPARAM)c.label.c_str());
 int chosen=0;for(int i=0;i<(int)captures.size();++i)if(captures[(size_t)i].label==prev){chosen=i;break;}
 if(!captures.empty()){select(sourceChoice,chosen);selectedCaptureLabel=captures[(size_t)chosen].label;}
}
static void listMicrophones(){
 int old=selection(audio);std::wstring prior=(old>0&&old<=(int)microphoneDevices.size())?microphoneDevices[(size_t)old-1]:L"";
 microphoneDevices.clear();SendMessageW(audio,CB_RESETCONTENT,0,0);
 SendMessageW(audio,CB_ADDSTRING,0,(LPARAM)L"No microphone");
 UINT count=waveInGetNumDevs();
 for(UINT i=0;i<count;i++){WAVEINCAPSW caps{};
  if(waveInGetDevCapsW(i,&caps,sizeof(caps))==MMSYSERR_NOERROR){
   std::wstring name=caps.szPname;microphoneDevices.push_back(name);
   SendMessageW(audio,CB_ADDSTRING,0,(LPARAM)name.c_str());
  }
 }
 int chosen=0;
 for(size_t i=0;i<microphoneDevices.size();++i)if(microphoneDevices[i]==prior)chosen=(int)i+1;
 select(audio,chosen);
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
 requestedFPS=fr;
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
  dest=newRecordingName(dest);
  if(dest.empty()){MessageBoxW(root,L"Could not find an unused recording filename.",L"LiteSR",MB_ICONWARNING);return;}
  if(!diskReady(dest,rate))return;
  SetWindowTextW(fileName,dest.c_str());
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
 progressPath=cfg();progressPath=progressPath.substr(0,progressPath.find_last_of(L"\\/")+1)+L"capture-progress.txt";
 DeleteFileW(progressPath.c_str());
 std::vector<std::wstring> args={ff,L"-hide_banner",L"-loglevel",L"error",L"-stats_period",L"1",L"-progress",progressPath};
 usingDDA=selection(captureBackend)==1&&selected.display;
 if(usingDDA){
  // Windows Desktop Duplication. FFmpeg's ddagrab returns D3D11 frames;
  // copy to CPU memory for the current encoder compatibility pipeline.
  int monitorIndex=selection(sourceChoice);
  args.insert(args.end(),{L"-filter_complex",L"ddagrab=framerate="+std::to_wstring(fr)+L":output_idx="+std::to_wstring(monitorIndex)+L",hwdownload,format=bgra[v]"});
 }else{
  args.insert(args.end(),{L"-f",L"gdigrab",L"-framerate",std::to_wstring(fr)});
  if(selected.display){
   args.insert(args.end(),{L"-offset_x",std::to_wstring(selected.bounds.left),L"-offset_y",std::to_wstring(selected.bounds.top),
   L"-video_size",std::to_wstring(selected.bounds.right-selected.bounds.left)+L"x"+std::to_wstring(selected.bounds.bottom-selected.bounds.top),L"-i",L"desktop"});
  }else{
   wchar_t handle[40]{};swprintf_s(handle,40,L"hwnd=0x%llx",(unsigned long long)(uintptr_t)selected.window);
   args.insert(args.end(),{L"-i",handle});
  }
 }
 // The DirectShow microphone is a second FFmpeg input, mapped explicitly.
 int micIndex=selection(audio);bool mic=micIndex>0&&micIndex<=(int)microphoneDevices.size();
 if(mic){
  args.insert(args.end(),{L"-f",L"dshow",L"-audio_buffer_size",L"100",L"-i",L"audio="+microphoneDevices[(size_t)micIndex-1]});
 }else if(isStreaming){
  args.insert(args.end(),{L"-f",L"lavfi",L"-i",L"anullsrc=channel_layout=stereo:sample_rate=44100"});
 }
 if(usingDDA)args.insert(args.end(),{L"-map",L"[v]"});
 else args.insert(args.end(),{L"-map",L"0:v:0"});
 if(mic||isStreaming)args.insert(args.end(),{L"-map",usingDDA?L"0:a:0":L"1:a:0"});
 else args.push_back(L"-an");
 args.insert(args.end(),{L"-vf",L"scale="+std::to_wstring(widths[r])+L":"+std::to_wstring(heights[r])+L",format=nv12"});
 if(selection(encoder)==0&&!isStreaming)args.insert(args.end(),{L"-c:v",L"h264_qsv",L"-preset",L"veryfast"});
 else args.insert(args.end(),{L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-tune",L"zerolatency"});
 args.insert(args.end(),{L"-b:v",std::to_wstring(rate)+L"k",L"-maxrate",std::to_wstring(rate)+L"k",
  L"-bufsize",std::to_wstring(rate*2)+L"k",L"-g",std::to_wstring(fr*2)});
 if(mic||isStreaming)args.insert(args.end(),{L"-c:a",L"aac",L"-b:a",L"128k",L"-ar",L"44100"});
 if(isStreaming){
  // FLV/RTMP compatibility defaults and low-latency network I/O.
  args.insert(args.end(),{L"-pix_fmt",L"yuv420p",L"-flvflags",L"no_duration_filesize",L"-f",L"flv",dest});
 }
 else args.insert(args.end(),{L"-n",L"-movflags",L"+faststart",dest});
 std::wstring cmd;
 for(const auto& a:args){if(!cmd.empty())cmd+=L" ";cmd+=quote(a);}
 SECURITY_ATTRIBUTES sa={sizeof(sa),nullptr,TRUE};HANDLE read{},write{};
 if(!CreatePipe(&read,&write,&sa,0)){setStatus(L"Could not create FFmpeg stdin.");return;}
 SetHandleInformation(write,HANDLE_FLAG_INHERIT,0);
 logPath=cfg();logPath=logPath.substr(0,logPath.find_last_of(L"\\/")+1)+L"ffmpeg.log";
 SECURITY_ATTRIBUTES logSecurity{sizeof(logSecurity),nullptr,TRUE};
 logHandle=CreateFileW(logPath.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&logSecurity,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
 if(logHandle==INVALID_HANDLE_VALUE){logHandle=nullptr;CloseHandle(read);CloseHandle(write);setStatus(L"Cannot create diagnostic log.");return;}
 STARTUPINFOW si{};si.cb=sizeof(si);si.dwFlags=STARTF_USESTDHANDLES;si.hStdInput=read;
 si.hStdOutput=logHandle;si.hStdError=logHandle;
 std::vector<wchar_t> buffer(cmd.begin(),cmd.end());buffer.push_back(0);
 BOOL ok=CreateProcessW(ff.c_str(),buffer.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&process);
 CloseHandle(read);
 if(!ok){CloseHandle(write);if(logHandle){CloseHandle(logHandle);logHandle=nullptr;}process={};setStatus(L"Failed launching FFmpeg.");return;}
 stdinPipe=write;stopRequested=false;stopRequestedAt=0;
 activeOutput=isStreaming?L"":dest;captureStarted=GetTickCount64();captureStreaming=isStreaming;
 {SYSTEMTIME now{};GetLocalTime(&now);wchar_t when[48]{};swprintf_s(when,48,L"%04d-%02d-%02d %02d:%02d:%02d",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);sessionStartedAt=when;}
 sessionSource=selected.label;sessionBackend=usingDDA?L"Desktop Duplication":L"GDI";sessionEncoder=selection(encoder)==0?L"Intel QSV":L"x264";sessionResolution=heights[r];sessionTargetFPS=fr;
 save();setStatus(isStreaming?L"Stream launched. Confirm live signal on platform.":L"Recording started.");refresh();
}
static ULONGLONG lastUpdateCheck=0;
static void checkUpdates(bool manual){
 if(!manual&&lastUpdateCheck!=0&&GetTickCount64()-lastUpdateCheck<3600000)return;
 lastUpdateCheck=GetTickCount64();
 std::wstring script=directory()+L"updater\\update.ps1";
 if(GetFileAttributesW(script.c_str())==INVALID_FILE_ATTRIBUTES){if(manual)MessageBoxW(root,L"Updater script is missing. Reinstall LiteSR.",L"Updates",MB_ICONWARNING);return;}
 std::wstring command=L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File "+
 quote(script)+L" -Channel beta -ParentPid "+std::to_wstring(GetCurrentProcessId())+L" -CurrentVersion "+buildVersion;
 if(manual)command+=L" -Manual";
 STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
 std::vector<wchar_t> buffer(command.begin(),command.end());buffer.push_back(0);
 if(CreateProcessW(nullptr,buffer.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi)){
  CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 }else if(manual)MessageBoxW(root,L"Could not start the updater. Check that PowerShell is available.",L"LiteSR Updates",MB_ICONWARNING);
}
static HWND settingsAdd(const wchar_t* cls,const wchar_t* value,DWORD style,int x,int y,int w,int h,int id=0){
 HWND result=add(cls,value,style,x,y,w,h,id);
 settingsPage.push_back(result);
 return result;
}
static HWND settingsCombo(int x,int y,int w,const std::vector<const wchar_t*>& options,int index){
 HWND c=settingsAdd(L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,x,y,w,220);
 for(auto t:options)SendMessageW(c,CB_ADDSTRING,0,(LPARAM)t);
 select(c,index);return c;
}
static void saveSettingsFromWindow(){
 int rate=0;try{rate=std::stoi(get(settingsBitrate));}catch(...){}
 if(rate<300||rate>50000){MessageBoxW(root,L"Bitrate must be between 300 and 50000 Kbps.",L"LiteSR Settings",MB_ICONWARNING);return;}
 SetWindowTextW(server,get(settingsServer).c_str());
 select(resolution,selection(settingsResolution));
 select(fps,selection(settingsFps));
 SetWindowTextW(bitrate,get(settingsBitrate).c_str());
 select(encoder,selection(settingsEncoder));
 select(captureBackend,selection(settingsBackend));
 highContrast=SendMessageW(settingsContrast,BM_GETCHECK,0,0)==BST_CHECKED;
 largeText=SendMessageW(settingsLargeText,BM_GETCHECK,0,0)==BST_CHECKED;
 reducedPreview=SendMessageW(settingsReducedPreview,BM_GETCHECK,0,0)==BST_CHECKED;
 reduceMotion=true;
 gentleAnimations=SendMessageW(settingsAnimations,BM_GETCHECK,0,0)==BST_CHECKED;
 save();applyTheme();SetTimer(root,1,reducedPreview?2500:1000,nullptr);
 settingsVisible=false;
 navigationStep=0;navigationAnimating=gentleAnimations;if(navigationAnimating)SetTimer(root,3,38,nullptr);
 for(HWND h:settingsPage)ShowWindow(h,SW_HIDE);
 for(HWND h:studioPage)if(h&&h!=updates)ShowWindow(h,SW_SHOW);
 // Technical controls are still backing state, but never shown in Studio.
 for(HWND h:{server,resolution,fps,bitrate,encoder,captureBackend,updates})ShowWindow(h,SW_HIDE);
 if(pageTitle)SetWindowTextW(pageTitle,L"LiteSR  /  STUDIO");
 InvalidateRect(root,nullptr,TRUE);
}
static void showSettings(){
 if(settingsVisible)return;
 settingsVisible=true;
 navigationStep=0;navigationAnimating=gentleAnimations;if(navigationAnimating)SetTimer(root,3,38,nullptr);

 for(HWND h:studioPage)ShowWindow(h,SW_HIDE);
 if(settingsPage.empty()){
  settingsBack=settingsAdd(L"BUTTON",L"Back to Studio",BS_PUSHBUTTON,28,19,195,39,102);
  settingsAdd(L"STATIC",L"SETTINGS  /  LITESR",0,256,20,760,36);
  settingsAdd(L"STATIC",L"STREAMING  &  OUTPUT",0,58,100,700,30);
  settingsAdd(L"STATIC",L"Server URL",0,58,154,180,28);
  settingsServer=settingsAdd(L"EDIT",get(server).c_str(),WS_BORDER|ES_AUTOHSCROLL,310,149,810,33);
  settingsAdd(L"STATIC",L"Resolution",0,58,218,180,28);
  settingsResolution=settingsCombo(310,214,375,{L"480p   854 x 480",L"720p   1280 x 720",L"1080p   1920 x 1080"},selection(resolution));
  settingsAdd(L"STATIC",L"Frame rate",0,710,218,150,28);
  settingsFps=settingsCombo(875,214,245,{L"30 FPS",L"60 FPS"},selection(fps));
  settingsAdd(L"STATIC",L"Bitrate (Kbps)",0,58,280,210,28);
  settingsBitrate=settingsAdd(L"EDIT",get(bitrate).c_str(),WS_BORDER|ES_NUMBER,310,275,375,33);
  settingsAdd(L"STATIC",L"Video encoder",0,710,280,150,28);
  settingsEncoder=settingsCombo(875,275,245,{L"Intel Quick Sync",L"CPU x264"},selection(encoder));
  settingsAdd(L"STATIC",L"Capture backend",0,58,344,210,28);
  settingsBackend=settingsCombo(310,340,810,{L"GDI  -  compatible",L"Desktop Duplication  -  experimental"},selection(captureBackend));
  settingsAdd(L"STATIC",L"ACCESSIBILITY  &  PERFORMANCE",0,58,421,900,32);
  settingsContrast=settingsAdd(L"BUTTON",L"High-contrast colors",BS_AUTOCHECKBOX,63,479,720,35);
  settingsLargeText=settingsAdd(L"BUTTON",L"Larger user interface text",BS_AUTOCHECKBOX,63,534,720,35);
  settingsReducedPreview=settingsAdd(L"BUTTON",L"Reduce preview refresh rate to save CPU",BS_AUTOCHECKBOX,63,589,900,35);
  settingsAnimations=settingsAdd(L"BUTTON",L"Gentle navigation highlight (no flashing or pulsing)",BS_AUTOCHECKBOX,63,640,980,35);
  settingsAdd(L"STATIC",L"Changes apply to the next recording. Stream keys are not stored.",0,58,708,1000,31);
  settingsAdd(L"BUTTON",L"View updater log",BS_PUSHBUTTON,58,758,245,48,13);
  settingsAdd(L"BUTTON",L"Save settings",BS_PUSHBUTTON,873,758,250,48,101);
 }
 SetWindowTextW(settingsServer,get(server).c_str());
 select(settingsResolution,selection(resolution));select(settingsFps,selection(fps));
 SetWindowTextW(settingsBitrate,get(bitrate).c_str());select(settingsEncoder,selection(encoder));
 select(settingsBackend,selection(captureBackend));
 SendMessageW(settingsContrast,BM_SETCHECK,highContrast?BST_CHECKED:BST_UNCHECKED,0);
 SendMessageW(settingsLargeText,BM_SETCHECK,largeText?BST_CHECKED:BST_UNCHECKED,0);
 SendMessageW(settingsReducedPreview,BM_SETCHECK,reducedPreview?BST_CHECKED:BST_UNCHECKED,0);
 SendMessageW(settingsAnimations,BM_SETCHECK,gentleAnimations?BST_CHECKED:BST_UNCHECKED,0);

 for(HWND h:settingsPage)ShowWindow(h,SW_SHOW);
 layoutWorkspace();InvalidateRect(root,nullptr,TRUE);
}
static LRESULT CALLBACK wnd(HWND h,UINT m,WPARAM w,LPARAM l){
 switch(m){
 case WM_CREATE:{
  root=h;
  largeFont=CreateFontW(-21,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  uiFont=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
  darkMode=(load(L"theme",L"dark")!=L"light");applyTheme();
  pageTitle=add(L"STATIC",L"LiteSR  /  STUDIO",0,22,12,480,32);
  add(L"STATIC",L"RECORD  /  STREAM  /  CREATE",0,530,18,300,25);
  add(L"BUTTON",L"Settings",BS_PUSHBUTTON,1034,14,193,32,12);
  add(L"STATIC",L"PREVIEW  -  selected capture source",0,27,58,560,28);
  previewButton=add(L"BUTTON",L"Preview: ON",BS_PUSHBUTTON,730,58,170,29,7);
  captureBackend=combo(530,58,190,{L"GDI (compatible)",L"Desktop Duplication"});
  add(L"STATIC",L"OUTPUT",0,928,60,300,28);
  add(L"STATIC",L"Output",0,929,101,85,24);
  mode=combo(1022,97,206,{L"Record MP4",L"Live stream"});
  add(L"STATIC",L"Platform",0,929,144,85,24);
  service=combo(1022,140,206,{L"YouTube",L"Twitch",L"Custom RTMP"});
  add(L"STATIC",L"Server URL",0,929,187,120,22);
  server=add(L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,928,210,300,28);
  add(L"STATIC",L"Stream key",0,929,250,120,22);
  key=add(L"EDIT",L"",WS_BORDER|ES_PASSWORD|ES_AUTOHSCROLL,928,274,300,28);
  add(L"STATIC",L"Recording filename",0,929,317,280,22);
  fileName=add(L"EDIT",L"recording.mp4",WS_BORDER|ES_AUTOHSCROLL,928,341,222,28);
  browseButton=add(L"BUTTON",L"Browse",BS_PUSHBUTTON,1157,341,71,28,8);
  add(L"STATIC",L"QUALITY PRESET",0,929,377,120,22);
  qualityPreset=combo(1051,372,177,{L"Low impact",L"Balanced",L"High quality"},1);
  add(L"STATIC",L"Resolution",0,929,418,120,22);
  resolution=combo(1045,412,183,{L"480p 854x480",L"720p 1280x720",L"1080p 1920x1080"},1);
  add(L"STATIC",L"FPS",0,929,458,100,22);
  fps=combo(1045,451,183,{L"30 FPS",L"60 FPS"});
  add(L"STATIC",L"Bitrate kbps",0,929,491,130,22);
  bitrate=add(L"EDIT",L"3000",WS_BORDER|ES_NUMBER,1045,488,183,27);
  add(L"STATIC",L"Encoder",0,929,532,120,22);
  encoder=combo(1045,528,183,{L"Intel Quick Sync",L"CPU x264"});
  add(L"STATIC",L"SCENES",0,28,585,192,26);
  add(L"STATIC",L"Main scene",0,30,630,194,26);
  add(L"STATIC",L"SOURCES",0,251,585,304,26);
  sourceMode=combo(260,620,290,{L"Display capture",L"Window capture"});
  sourceChoice=combo(260,669,290,{});
  add(L"BUTTON",L"Refresh sources",BS_PUSHBUTTON,260,716,290,34,4);
  add(L"STATIC",L"AUDIO MIXER",0,581,585,290,28);
  add(L"STATIC",L"MICROPHONE  /  DIRECTSHOW",0,589,639,279,26);
  add(L"STATIC",L"Audio source for recordings and streams.",0,589,678,284,45);
  audio=combo(590,736,211,{L"No microphone"});
  audioRefreshButton=add(L"BUTTON",L"Scan",BS_PUSHBUTTON,805,736,61,28,10);
  add(L"STATIC",L"CONTROLS",0,901,585,324,28);
  startButton=add(L"BUTTON",L"Start recording / streaming",BS_PUSHBUTTON|WS_TABSTOP,906,621,318,46,1);
  stopButton=add(L"BUTTON",L"Stop and finalize",BS_PUSHBUTTON|WS_TABSTOP,906,677,318,43,2);
  updates=combo(906,737,155,{L"Beta / Alpha"},0);
  ShowWindow(updates,SW_HIDE);
  add(L"BUTTON",L"Check updates",BS_PUSHBUTTON,1071,737,153,32,3);
  add(L"BUTTON",L"View FFmpeg log",BS_PUSHBUTTON,906,779,152,32,5);
  historyButton=add(L"BUTTON",L"Session history",BS_PUSHBUTTON,906,822,152,31,14);
  openFolderButton=add(L"BUTTON",L"Open recordings",BS_PUSHBUTTON,1070,779,152,32,9);
  diskInfo=add(L"STATIC",L"Free disk: checking",0,580,791,307,27);
  captureHealth=add(L"STATIC",L"ENCODE FPS: waiting for capture",0,28,791,530,27);
  themeButton=add(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,1070,823,154,32,6);
  state=add(L"STATIC",L"Ready  |  FFmpeg included",0,27,860,780,30);
  sessionInfo=add(L"STATIC",L"IDLE  |  F9 start / F10 stop",0,820,860,420,30);
  applyTheme();
  select(mode,loadIndex(L"mode",0,2));select(service,loadIndex(L"service",0,3));
  serviceChanged();
  auto savedServer=load(L"server",L"");if(!savedServer.empty())SetWindowTextW(server,savedServer.c_str());
  std::wstring defaultOutput=recordingsFolder()+L"\\recording.mp4";
  SetWindowTextW(fileName,load(L"output",defaultOutput.c_str()).c_str());
  select(resolution,loadIndex(L"resolution",1,3));select(fps,loadIndex(L"fps",0,2));
  SetWindowTextW(bitrate,load(L"bitrate",L"3000").c_str());
  select(encoder,loadIndex(L"encoder",0,2));select(updates,0);
  highContrast=load(L"highContrast",L"0")==L"1";largeText=load(L"largeText",L"0")==L"1";reducedPreview=load(L"reducedPreview",L"0")==L"1";reduceMotion=true;gentleAnimations=load(L"gentleAnimations",L"0")==L"1";applyTheme();
  selectedCaptureLabel=load(L"sourceName",L"");previewEnabled=load(L"preview",L"1")!=L"0";
  SetWindowTextW(previewButton,previewEnabled?L"Preview: ON":L"Preview: OFF");
  select(captureBackend,loadIndex(L"backend",0,2));
  select(sourceMode,loadIndex(L"sourceMode",0,2));listCapture();listMicrophones();
  auto micSaved=load(L"microphone",L"");for(size_t i=0;i<microphoneDevices.size();++i)if(microphoneDevices[i]==micSaved){select(audio,(int)i+1);break;}
  RegisterHotKey(h,91,MOD_NOREPEAT,VK_F9);RegisterHotKey(h,92,MOD_NOREPEAT,VK_F10);
  for(const auto& entry:uiPositions)studioPage.push_back(entry.h);
  // Technical controls live in Settings, not on the studio surface.
  for(const auto& entry:uiPositions){
   int x=entry.bounds.left,y=entry.bounds.top;
   if((x>=920&&((y==187||y==210)||(y>=372&&y<=555)))||(x==530&&y==58))ShowWindow(entry.h,SW_HIDE);
  }
  layoutReady=true;layoutWorkspace();
  SetTimer(h,1,reducedPreview?2500:1000,nullptr);SetTimer(h,2,3500,nullptr);refresh();return 0;
 }
 case WM_SIZE:if(layoutReady&&w!=SIZE_MINIMIZED)layoutWorkspace();return 0;
 case WM_KEYDOWN:if(w==VK_F11){toggleFullscreen();return 0;}if(w==VK_ESCAPE&&fullscreen){toggleFullscreen();return 0;}break;
 case WM_PAINT:{
  PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);
  if(navigationAnimating||gentleAnimations){RECT client{};GetClientRect(h,&client);RECT bar{0,47,client.right,53};
   HBRUSH track=CreateSolidBrush(panelColor);FillRect(dc,&bar,track);DeleteObject(track);
   int end=navigationAnimating?client.right*navigationStep/6:client.right;
   RECT fill{0,47,end,53};HBRUSH brush=CreateSolidBrush(accent);FillRect(dc,&fill,brush);DeleteObject(brush);
  }
  if(settingsVisible){EndPaint(h,&ps);return 0;}
  HBRUSH bg=CreateSolidBrush(darkMode?RGB(9,13,22):RGB(218,229,244));
  FillRect(dc,&previewArea,bg);DeleteObject(bg);
  if(process.hProcess){
   RECT dot={previewArea.left+14,previewArea.top+14,previewArea.left+28,previewArea.top+28};
   HBRUSH red=CreateSolidBrush(RGB(239,65,78));HGDIOBJ old=SelectObject(dc,red);
   Ellipse(dc,dot.left,dot.top,dot.right,dot.bottom);SelectObject(dc,old);DeleteObject(red);
  }
  int selectedIndex=sourceChoice?selection(sourceChoice):-1;
  if(!settingsVisible&&previewEnabled&&selectedIndex>=0&&selectedIndex<(int)captures.size()){
   const Capture& c=captures[(size_t)selectedIndex];
   RECT r=c.bounds;int sw=r.right-r.left,sh=r.bottom-r.top;
   if(sw>0&&sh>0){
    int vw=previewArea.right-previewArea.left-24,vh=previewArea.bottom-previewArea.top-24;
    double scale=std::min((double)vw/sw,(double)vh/sh);
    int dw=(int)(sw*scale),dh=(int)(sh*scale);
    int dx=previewArea.left+(previewArea.right-previewArea.left-dw)/2;
    int dy=previewArea.top+(previewArea.bottom-previewArea.top-dh)/2;
    HDC src=GetDC(nullptr);
    if(src){SetStretchBltMode(dc,HALFTONE);
      StretchBlt(dc,dx,dy,dw,dh,src,r.left,r.top,sw,sh,SRCCOPY);ReleaseDC(nullptr,src);}
   }
  }else{
   SetBkMode(dc,TRANSPARENT);SetTextColor(dc,darkMode?RGB(152,168,196):RGB(67,86,115));
   DrawTextW(dc,previewEnabled?L"Select a source below to preview your capture.":L"Preview paused - capture is still available.",-1,&previewArea,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
  }
  EndPaint(h,&ps);return 0;
 }
 case WM_ERASEBKGND:{RECT r;GetClientRect(h,&r);HBRUSH b=CreateSolidBrush(canvas);FillRect((HDC)w,&r,b);DeleteObject(b);return 1;}
 case WM_MEASUREITEM:{MEASUREITEMSTRUCT* mi=(MEASUREITEMSTRUCT*)l;mi->itemHeight=27;return TRUE;}
 case WM_DRAWITEM:{
  const DRAWITEMSTRUCT* item=(const DRAWITEMSTRUCT*)l;
  RECT r=item->rcItem;
  if(item->CtlType==ODT_COMBOBOX){
   HDC dc=item->hDC;HBRUSH bg=CreateSolidBrush(fieldColor);FillRect(dc,&r,bg);DeleteObject(bg);
   if(item->itemID!=(UINT)-1){
    wchar_t t[512]{};SendMessageW(item->hwndItem,CB_GETLBTEXT,item->itemID,(LPARAM)t);
    bool selected=(item->itemState&ODS_SELECTED)!=0;
    if(selected){HBRUSH b=CreateSolidBrush(darkMode?RGB(48,76,125):RGB(207,225,255));FillRect(dc,&r,b);DeleteObject(b);}
    SetTextColor(dc,ink);SetBkMode(dc,TRANSPARENT);r.left+=9;
    DrawTextW(dc,t,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
   }
   return TRUE;
  }
  if(item->CtlType==ODT_STATIC){
   HDC dc=item->hDC;
   wchar_t label[768]{};GetWindowTextW(item->hwndItem,label,768);
   bool heading=wcsstr(label,L"01   ")==label||wcsstr(label,L"02   ")==label||
    wcsstr(label,L"03   ")==label||wcsstr(label,L"04   ")==label;
   bool brand=wcsstr(label,L"LiteSR  /")==label;
   HBRUSH b=CreateSolidBrush(heading?panelColor:canvas);
   FillRect(dc,&r,b);DeleteObject(b);
   SetBkMode(dc,TRANSPARENT);SetTextColor(dc,(heading||brand)?accent:ink);
   RECT t=r;t.left+=heading?12:0;
   DrawTextW(dc,label,-1,&t,DT_LEFT|DT_VCENTER|DT_WORDBREAK|DT_END_ELLIPSIS);
   return TRUE;
  }
  if(item->CtlType==ODT_BUTTON){
   HDC dc=item->hDC;bool active=(item->itemState&ODS_SELECTED)!=0;
   bool disabled=(item->itemState&ODS_DISABLED)!=0;
   bool primary=item->CtlID==1;
   COLORREF bgColor=disabled?(darkMode?RGB(42,47,62):RGB(223,228,239)):
    primary?(active?RGB(60,104,209):accent):
    (active?(darkMode?RGB(59,76,110):RGB(208,223,248)):(darkMode?RGB(38,52,79):RGB(222,233,252)));
   HBRUSH b=CreateSolidBrush(bgColor);HPEN p=CreatePen(PS_SOLID,1,primary?accent:(darkMode?RGB(69,93,136):RGB(183,202,231)));
   HGDIOBJ ob=SelectObject(dc,b),op=SelectObject(dc,p);
   RoundRect(dc,r.left+1,r.top+1,r.right-1,r.bottom-1,14,14);
   SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(b);DeleteObject(p);
   SetBkMode(dc,TRANSPARENT);
   SetTextColor(dc,disabled?(darkMode?RGB(128,138,155):RGB(143,153,169)):primary?RGB(255,255,255):ink);
   wchar_t label[256]{};GetWindowTextW(item->hwndItem,label,256);
   DrawTextW(dc,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
   if(item->itemState&ODS_FOCUS){RECT fr=r;InflateRect(&fr,-4,-4);DrawFocusRect(dc,&fr);}
   return TRUE;
  }
  return FALSE;
 }
 case WM_CTLCOLORSTATIC:
 case WM_CTLCOLORBTN:
 case WM_CTLCOLOREDIT:
 case WM_CTLCOLORLISTBOX:{
  HDC dc=(HDC)w;SetTextColor(dc,ink);SetBkColor(dc,canvas);
  if(m==WM_CTLCOLOREDIT||m==WM_CTLCOLORLISTBOX){SetBkColor(dc,fieldColor);return (LRESULT)fieldBrush;}
  return (LRESULT)darkBrush;
 }
 case WM_HOTKEY:
  if(w==91&&!process.hProcess)start();
  if(w==92&&process.hProcess)stop();return 0;
 case WM_COMMAND:
  if(LOWORD(w)==14){auto p=sessionHistoryPath();if(GetFileAttributesW(p.c_str())==INVALID_FILE_ATTRIBUTES)MessageBoxW(root,L"No sessions have finished yet.",L"Session history",MB_OK);else ShellExecuteW(root,L"open",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL);return 0;}
  if(LOWORD(w)==13){std::wstring p=cfg();p=p.substr(0,p.find_last_of(L"\\/")+1)+L"updater.log";if(GetFileAttributesW(p.c_str())==INVALID_FILE_ATTRIBUTES)MessageBoxW(root,L"No updater log yet. Run Check updates first.",L"LiteSR",MB_OK);else ShellExecuteW(root,L"open",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL);return 0;}
  if(LOWORD(w)==101){saveSettingsFromWindow();return 0;}
  if(LOWORD(w)==102){settingsVisible=false;navigationStep=0;navigationAnimating=gentleAnimations;if(navigationAnimating)SetTimer(root,3,38,nullptr);for(HWND v:settingsPage)ShowWindow(v,SW_HIDE);for(HWND v:studioPage)if(v&&v!=updates)ShowWindow(v,SW_SHOW);for(HWND v:{server,resolution,fps,bitrate,encoder,captureBackend,updates})ShowWindow(v,SW_HIDE);InvalidateRect(root,nullptr,TRUE);return 0;}
  if(LOWORD(w)==1)start();
  if(LOWORD(w)==2)stop();
  if(LOWORD(w)==3)checkUpdates(true);
  if(LOWORD(w)==4||((HWND)l==sourceMode&&HIWORD(w)==CBN_SELCHANGE))listCapture();
  if((HWND)l==sourceChoice&&HIWORD(w)==CBN_SELCHANGE){int i=selection(sourceChoice);if(i>=0&&i<(int)captures.size())selectedCaptureLabel=captures[(size_t)i].label;save();InvalidateRect(root,&previewArea,FALSE);}
  if(LOWORD(w)==12)showSettings();
  if(LOWORD(w)==10){listMicrophones();save();}
  if((HWND)l==audio&&HIWORD(w)==CBN_SELCHANGE)save();
  if((HWND)l==captureBackend&&HIWORD(w)==CBN_SELCHANGE)save();
  if(LOWORD(w)==8)browseRecording();
  if(LOWORD(w)==9)openRecordingFolder();
  if((HWND)l==qualityPreset&&HIWORD(w)==CBN_SELCHANGE)applyPreset();
  if(LOWORD(w)==7){previewEnabled=!previewEnabled;SetWindowTextW(previewButton,previewEnabled?L"Preview: ON":L"Preview: OFF");save();InvalidateRect(h,&previewArea,FALSE);}
  if(LOWORD(w)==6){darkMode=!darkMode;applyTheme();save();}
  if(LOWORD(w)==5){std::wstring p=logPath.empty()?cfg().substr(0,cfg().find_last_of(L"\\/")+1)+L"ffmpeg.log":logPath;ShellExecuteW(h,L"open",p.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
  if((HWND)l==service&&HIWORD(w)==CBN_SELCHANGE)serviceChanged();
  return 0;
 case WM_TIMER:
  if(w==3){if(++navigationStep>=6){navigationAnimating=false;KillTimer(h,3);}RECT bar{0,0,0,0};GetClientRect(h,&bar);bar.top=47;bar.bottom=53;InvalidateRect(h,&bar,FALSE);return 0;}
  if(w==1){
   auto marker=cfg();marker=marker.substr(0,marker.find_last_of(L"\\/")+1)+L"update-ready.flag";
   if(GetFileAttributesW(marker.c_str())!=INVALID_FILE_ATTRIBUTES){
    char content[64]{};HANDLE fh=CreateFileW(marker.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    DWORD used=0;if(fh!=INVALID_HANDLE_VALUE){ReadFile(fh,content,sizeof(content)-1,&used,nullptr);CloseHandle(fh);}
    DeleteFileW(marker.c_str());
    if(std::string(content,content+used)==std::to_string(GetCurrentProcessId())){updateExitRequested=true;
      if(process.hProcess){stop();setStatus(L"Update approved - finalizing capture before installing...");}}

   }
  }
  if(w==2){KillTimer(h,2);checkUpdates(false);}else refresh();
  if(w==1&&!updateExitRequested)checkUpdates(false);
  if(w==1&&previewEnabled&&!settingsVisible)InvalidateRect(h,&previewArea,FALSE);
  return 0;
 case WM_CLOSE:
  if(process.hProcess){stop();MessageBoxW(h,L"Stop and finalize recording before closing.",L"LiteSR",MB_OK);return 0;}
  save();DestroyWindow(h);return 0;
 case WM_DESTROY:if(largeFont)DeleteObject(largeFont);UnregisterHotKey(h,91);UnregisterHotKey(h,92);if(darkBrush){DeleteObject(darkBrush);darkBrush=nullptr;}if(fieldBrush){DeleteObject(fieldBrush);fieldBrush=nullptr;}if(uiFont){DeleteObject(uiFont);uiFont=nullptr;}PostQuitMessage(0);return 0;
 }
 return DefWindowProcW(h,m,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){
 WNDCLASSW wc{};wc.hInstance=instance;wc.lpfnWndProc=wnd;wc.lpszClassName=L"LiteSRMain";
 wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
 RegisterClassW(&wc);
 HWND h=CreateWindowExW(0,L"LiteSRMain",L"LiteSR v0.2.0-alpha.26",WS_OVERLAPPEDWINDOW,
 CW_USEDEFAULT,CW_USEDEFAULT,1260,925,nullptr,nullptr,instance,nullptr);
 ShowWindow(h,show);MSG msg;
 while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
 return 0;
}
