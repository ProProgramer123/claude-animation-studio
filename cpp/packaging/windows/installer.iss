; Inno Setup script for the Windows installer. CI runs:
;   iscc /DSourceDir=<windeployqt output> /DAppVersion=<x.y.z> /DOutputDir=<dir> installer.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{6E1E8E5C-7B4B-4C55-9F0E-3D2D1E6B9A11}
AppName=Claude Animation Studio
AppVersion={#AppVersion}
AppPublisher=Claude Animation Studio
AppPublisherURL=https://github.com/ProProgramer123/claude-animation-studio
DefaultDirName={autopf}\Claude Animation Studio
DefaultGroupName=Claude Animation Studio
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=Claude-Animation-Studio-{#AppVersion}-Windows-Setup
SetupIconFile=..\..\app\resources\icon.ico
UninstallDisplayIcon={app}\Claude Animation Studio.exe
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequiredOverridesAllowed=dialog
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{autoprograms}\Claude Animation Studio"; Filename: "{app}\Claude Animation Studio.exe"
Name: "{autodesktop}\Claude Animation Studio"; Filename: "{app}\Claude Animation Studio.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\Claude Animation Studio.exe"; Description: "{cm:LaunchProgram,Claude Animation Studio}"; Flags: nowait postinstall skipifsilent
