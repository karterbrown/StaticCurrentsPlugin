Windows Setup Package (Installer)
=================================

This folder builds a Setup.exe for the standalone app.

1) Build the EXE in Visual Studio (Release x64).
   Output path:
   Builds\VisualStudio2022\x64\Release\Standalone Plugin\StaticCurrentsPlugin.exe

2) Copy the EXE into this folder:
   SetupPackage\StaticCurrentsPlugin.exe

3) Install Inno Setup 6:
   https://jrsoftware.org/isinfo.php

4) Run BuildInstaller.bat

5) The installer will be created here:
   SetupPackage\StaticCurrentsPluginSetup.exe
