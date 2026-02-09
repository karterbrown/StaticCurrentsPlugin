[Setup]
AppName=Static Currents Plugin
AppVersion=1.0.0
DefaultDirName={autopf}\Static Currents Plugin
DefaultGroupName=Static Currents Plugin
OutputDir=SetupPackage
OutputBaseFilename=StaticCurrentsPluginSetup_v1.0.0
Compression=lzma2/max
SolidCompression=yes
UninstallDisplayIcon={app}\StaticCurrentsPlugin.exe
AppPublisher=Karter Brown
AppCopyright=Copyright (C) 2026 Karter Brown
VersionInfoVersion=1.0.0.0
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Files]
Source: "PortablePackage\App\StaticCurrentsPlugin.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Static Currents Plugin"; Filename: "{app}\StaticCurrentsPlugin.exe"
Name: "{group}\Uninstall Static Currents Plugin"; Filename: "{uninstallexe}"
Name: "{autodesktop}\Static Currents Plugin"; Filename: "{app}\StaticCurrentsPlugin.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\StaticCurrentsPlugin.exe"; Description: "Launch Static Currents Plugin"; Flags: postinstall nowait skipifsilent
