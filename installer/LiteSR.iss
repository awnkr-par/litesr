#define AppVersion GetEnv("LITESR_VERSION")
#if AppVersion == ""
 #define AppVersion "0.2.0-alpha.18"
#endif
[Setup]
AppId={{F7417356-74F3-4EB1-BE66-21234A330226}
AppName=LiteSR
AppVersion={#AppVersion}
AppPublisher=LiteSR Contributors
AppPublisherURL=https://github.com/awnkr-par/litesr
DefaultDirName={localappdata}\Programs\LiteSR
DefaultGroupName=LiteSR
OutputDir=..\dist
OutputBaseFilename=LiteSR-{#AppVersion}-Setup-x64
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
WizardStyle=modern
[Files]
Source: "..\build\Release\LiteSR.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\litesr.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\updater\update.ps1"; DestDir: "{app}\updater"; Flags: ignoreversion
Source: "..\vendor\ffmpeg\*"; DestDir: "{app}\ffmpeg"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\LiteSR"; Filename: "{app}\LiteSR.exe"
Name: "{autodesktop}\LiteSR"; Filename: "{app}\LiteSR.exe"; Tasks: desktopicon
[Tasks]
Name: "desktopicon"; Description: "Create desktop shortcut"; Flags: unchecked
[Run]
Filename: "{app}\LiteSR.exe"; Description: "Launch LiteSR"; Flags: nowait postinstall skipifsilent
