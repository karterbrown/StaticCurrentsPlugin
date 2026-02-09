param(
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    [ValidateSet("x64", "Win32")]
    [string]$Platform = "x64",
    [switch]$BuildInstaller
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$solution = Join-Path $root "Builds\VisualStudio2022\Static Currents Plugin.sln"
$standaloneProjectFile = Join-Path $root "Builds\VisualStudio2022\Static Currents Plugin_StandalonePlugin.vcxproj"
$sharedProjectFile = Join-Path $root "Builds\VisualStudio2022\Static Currents Plugin_SharedCode.vcxproj"
$exeOut = Join-Path $root "Builds\VisualStudio2022\$Platform\$Configuration\Standalone Plugin\StaticCurrentsPlugin.exe"
$portableDest = Join-Path $root "PortablePackage\App\StaticCurrentsPlugin.exe"
$setupDest = Join-Path $root "SetupPackage\StaticCurrentsPlugin.exe"
$installerBat = Join-Path $root "SetupPackage\BuildInstaller.bat"

if (-not (Test-Path $solution)) {
    throw "Solution not found: $solution"
}
if (-not (Test-Path $standaloneProjectFile)) {
    throw "Standalone project not found: $standaloneProjectFile"
}
if (-not (Test-Path $sharedProjectFile)) {
    throw "Shared code project not found: $sharedProjectFile"
}

# Locate MSBuild via vswhere (VS 2017+). Fall back to common MSBuild paths.
$msbuild = $null
$vswhereCandidates = @(
    "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\Installer\vswhere.exe"
)
foreach ($vswhere in $vswhereCandidates) {
    if (Test-Path $vswhere) {
        $msbuild = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if (-not $msbuild) {
            $installPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
            if ($installPath) {
                $candidate = Join-Path $installPath "MSBuild\Current\Bin\MSBuild.exe"
                if (Test-Path $candidate) { $msbuild = $candidate }
            }
        }
        if ($msbuild) { break }
    }
}
if (-not $msbuild) {
    $fallbacks = @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )
    $msbuild = $fallbacks | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $msbuild) {
    $cmd = Get-Command msbuild -ErrorAction SilentlyContinue
    if ($cmd) { $msbuild = $cmd.Path }
}
if (-not $msbuild) {
    throw "MSBuild not found. Install Visual Studio 2022 with Desktop development with C++ or Build Tools."
}

# Use the newest installed MSVC toolset version to avoid missing-toolset errors.
$vcToolsVersion = $null
$vcToolsInstallDir = $null
$vcToolsRedistDir = $null
try {
    $vsInstallDir = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $msbuild)))
    $vcToolsRoot = Join-Path $vsInstallDir "VC\Tools\MSVC"
    if (Test-Path $vcToolsRoot) {
        $allVcTools = Get-ChildItem $vcToolsRoot -Directory
        $preferredVcTools = $allVcTools | Where-Object { $_.Name -like "14.3*" }
        if ($preferredVcTools) {
            $vcToolsVersion = $preferredVcTools |
                Sort-Object { [version]$_.Name } -Descending |
                Select-Object -First 1 -ExpandProperty Name
            $vcToolsInstallDir = Join-Path $vcToolsRoot $vcToolsVersion
            $vcRedistRoot = Join-Path $vsInstallDir "VC\Redist\MSVC"
            if (Test-Path $vcRedistRoot) {
                $vcToolsRedistDir = Join-Path $vcRedistRoot $vcToolsVersion
            }
        }
    }
} catch {
    $vcToolsVersion = $null
    $vcToolsInstallDir = $null
    $vcToolsRedistDir = $null
}

Write-Host "Building Standalone Plugin ($Configuration|$Platform)..."
$baseMsbuildArgs = @(
    "/m",
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform"
)
$msbuildArgs = @($sharedProjectFile) + $baseMsbuildArgs
if ($vcToolsVersion) {
    Write-Host "Using VCToolsVersion: $vcToolsVersion"
    $msbuildArgs += "/p:VCToolsVersion=$vcToolsVersion"
    $baseMsbuildArgs += "/p:VCToolsVersion=$vcToolsVersion"
    if ($vcToolsInstallDir -and (Test-Path $vcToolsInstallDir)) {
        $msbuildArgs += "/p:VCToolsInstallDir=`"$vcToolsInstallDir`""
        $baseMsbuildArgs += "/p:VCToolsInstallDir=`"$vcToolsInstallDir`""
    }
    if ($vcToolsRedistDir -and (Test-Path $vcToolsRedistDir)) {
        $msbuildArgs += "/p:VCToolsRedistDir=`"$vcToolsRedistDir`""
        $baseMsbuildArgs += "/p:VCToolsRedistDir=`"$vcToolsRedistDir`""
    }
}
Write-Host "Building Shared Code ($Configuration|$Platform)..."
& $msbuild @msbuildArgs
if ($LASTEXITCODE -ne 0) {
    throw "Shared code build failed. See MSBuild output above."
}

$msbuildArgs = @($standaloneProjectFile) + $baseMsbuildArgs
Write-Host "Building Standalone Plugin ($Configuration|$Platform)..."
& $msbuild @msbuildArgs
if ($LASTEXITCODE -ne 0) {
    throw "Build failed. See MSBuild output above."
}

if (-not (Test-Path $exeOut)) {
    throw "Build succeeded but EXE not found at: $exeOut"
}

Write-Host "Copying EXE to portable and setup folders..."
New-Item -ItemType Directory -Force (Split-Path $portableDest) | Out-Null
Copy-Item -Force $exeOut $portableDest
Copy-Item -Force $exeOut $setupDest

if ($BuildInstaller) {
    if (-not (Test-Path $installerBat)) {
        throw "Installer script not found: $installerBat"
    }
    Write-Host "Running installer build..."
    Push-Location (Split-Path $installerBat)
    try {
        cmd /c $installerBat
    } finally {
        Pop-Location
    }
}

Write-Host "Done. Portable EXE: $portableDest"
if ($BuildInstaller) {
    Write-Host "Installer output: $root\SetupPackage\StaticCurrentsPluginSetup.exe"
}
