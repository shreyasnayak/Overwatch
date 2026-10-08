[Setup]
AppName=Overwatch
AppVersion=0.0.0.1
AppPublisher=Interlink Network Systems, Inc.
WizardStyle=modern
DefaultDirName={pf32}\Syntax Masons\Overwatch 
DefaultGroupName=Overwatch
UninstallDisplayIcon={app}\interlink.ico
Compression=lzma2
SolidCompression=yes
OutputDir=..\Package_Build
OutputBaseFilename={#SetupSetting("AppName")}_i386_{#SetupSetting("AppVersion")}
DisableWelcomePage=no
PrivilegesRequired=admin

[Files]
Source: "Overwatch_Build\*"; DestDir: "{app}\"; Flags: ignoreversion recursesubdirs

[Icons]
Name: "{group}\Overwatch"; Filename: "{app}\Overwatch.exe" 

[UninstallRun]
Filename: "taskkill"; Parameters: "/F /IM Overwatch.exe"; WorkingDir: "{app}"; Flags: shellexec waituntilterminated

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
Type: filesandordirs; Name: "{userappdata}\Syntax Masons\Overwatch"