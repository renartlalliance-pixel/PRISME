$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
$juce = "C:\Users\Utilisateur\Desktop\prisme\PrismeMix\build\_deps\juce-src"

# Version 4.0.0 (idempotent)
$cm = Get-Content "$root\CMakeLists.txt" -Raw
$cm2 = $cm -replace 'project\(Prisme VERSION [0-9.]+ ', 'project(Prisme VERSION 4.0.0 '
if ($cm2 -ne $cm) { Set-Content "$root\CMakeLists.txt" $cm2 -NoNewline; Write-Host "Version CMake -> 4.0.0" }

# Environnement Visual Studio
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -property installationPath
Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64"
Set-Location $root

$extra = @()
if (Test-Path "$juce\modules") { $extra += "-DFETCHCONTENT_SOURCE_DIR_JUCE=$juce"; Write-Host "JUCE local reutilise" }

cmake -S . -B build_v4 -G "Visual Studio 17 2022" -A x64 @extra
if ($LASTEXITCODE -ne 0) { Write-Host "ECHEC_CONFIGURE"; exit 1 }

cmake --build build_v4 --config Release --target Prisme_Standalone --parallel
if ($LASTEXITCODE -ne 0) { Write-Host "ECHEC_COMPILATION"; exit 2 }

$exe = Get-ChildItem "$root\build_v4" -Recurse -Filter "Prisme.exe" | Where-Object { $_.FullName -match "Standalone" } | Select-Object -First 1
if (-not $exe) { Write-Host "ECHEC_EXE_INTROUVABLE"; exit 3 }
$dest = Join-Path ([Environment]::GetFolderPath("Desktop")) "PRISME_v4_test.exe"
Copy-Item $exe.FullName $dest -Force
Write-Host "BUILD_OK $dest"

