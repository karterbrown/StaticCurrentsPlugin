[Setup]
AppName=Static Currents Plugin
AppVersion=1.0.0
DefaultDirName={pf}\Static Currents Plugin
DefaultGroupName=Static Currents Plugin
OutputDir=.
OutputBaseFilename=StaticCurrentsPluginSetup
Compression=lzma
SolidCompression=yes

[Files]
Source: "StaticCurrentsPlugin.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Static Currents Plugin"; Filename: "{app}\StaticCurrentsPlugin.exe"
Name: "{commondesktop}\Static Currents Plugin"; Filename: "{app}\StaticCurrentsPlugin.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked
