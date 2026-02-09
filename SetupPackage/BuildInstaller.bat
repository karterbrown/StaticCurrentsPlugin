@echo off
setlocal

set "EXE=StaticCurrentsPlugin.exe"
if not exist "%EXE%" (
  echo ERROR: %EXE% not found in %CD%
  echo Copy the built EXE into this folder and try again.
  exit /b 1
)

set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"

if not exist "%ISCC%" (
  echo ERROR: Inno Setup 6 not found.
  echo Install it from https://jrsoftware.org/isinfo.php
  exit /b 1
)

"%ISCC%" installer.iss
if errorlevel 1 exit /b 1

echo Done. Output: %CD%\StaticCurrentsPluginSetup.exe
