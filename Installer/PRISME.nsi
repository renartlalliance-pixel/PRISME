Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"
!include "x64.nsh"

!ifndef BUILD_DIR
  !define BUILD_DIR "..\build\Prisme_artefacts\Release"
!endif
!ifndef OUTPUT_FILE
  !define OUTPUT_FILE "PRISME-Setup.exe"
!endif

Name "PRISME"
OutFile "${OUTPUT_FILE}"
InstallDir "$PROGRAMFILES64\Atelier Prisme\Prisme"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show
VIProductVersion "4.0.0.0"
VIAddVersionKey /LANG=1036 "ProductName" "PRISME"
VIAddVersionKey /LANG=1036 "FileDescription" "Installation et mise à jour de PRISME"
VIAddVersionKey /LANG=1036 "FileVersion" "4.0.0"
VIAddVersionKey /LANG=1036 "ProductVersion" "4.0.0"
VIAddVersionKey /LANG=1036 "CompanyName" "Atelier Prisme"
VIAddVersionKey /LANG=1036 "LegalCopyright" "Atelier Prisme"

!define MUI_WELCOMEPAGE_TITLE "Installer ou mettre à jour PRISME"
!define MUI_WELCOMEPAGE_TEXT "Cet assistant installe le plug-in VST3 64 bits de PRISME avec ses douze skins. Vous pouvez aussi installer l'application autonome.$\r$\n$\r$\nFermez PRISME et votre logiciel audio avant de poursuivre."
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "French"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP "PRISME nécessite Windows 64 bits."
    Abort
  ${EndIf}
  SetRegView 64
  SetShellVarContext all
FunctionEnd

Section "Plug-in VST3 x64" SEC_VST
  SectionIn RO
  SetOutPath "$COMMONFILES64\VST3\Prisme.vst3\Contents\x86_64-win"
  File "/oname=Prisme.staging" "${BUILD_DIR}\VST3\Prisme.vst3\Contents\x86_64-win\Prisme.vst3"
  StrCpy $0 "$OUTDIR\Prisme.staging"
  StrCpy $1 "$OUTDIR\Prisme.vst3"
  Call ReplacePayload
  SetOutPath "$COMMONFILES64\VST3\Prisme.vst3\Contents\Resources"
  File "/oname=moduleinfo.staging" "${BUILD_DIR}\VST3\Prisme.vst3\Contents\Resources\moduleinfo.json"
  StrCpy $0 "$OUTDIR\moduleinfo.staging"
  StrCpy $1 "$OUTDIR\moduleinfo.json"
  Call ReplacePayload
  SetOutPath "$INSTDIR"
  WriteUninstaller "$INSTDIR\Desinstaller-PRISME.exe"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AtelierPrismeVST3" "DisplayName" "PRISME"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AtelierPrismeVST3" "DisplayVersion" "4.0.0"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AtelierPrismeVST3" "Publisher" "Atelier Prisme"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AtelierPrismeVST3" "UninstallString" '"$INSTDIR\Desinstaller-PRISME.exe"'
SectionEnd

Section "Application autonome x64" SEC_APP
  SetOutPath "$INSTDIR"
  File "/oname=Prisme.staging" "${BUILD_DIR}\Standalone\Prisme.exe"
  StrCpy $0 "$OUTDIR\Prisme.staging"
  StrCpy $1 "$OUTDIR\Prisme.exe"
  Call ReplacePayload
  CreateDirectory "$SMPROGRAMS\Atelier Prisme\Prisme"
  CreateShortcut "$SMPROGRAMS\Atelier Prisme\Prisme\PRISME.lnk" "$INSTDIR\Prisme.exe"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AtelierPrismeVST3" "Standalone" 1
SectionEnd

Function ReplacePayload
retry:
  System::Call 'kernel32::MoveFileExW(w r0, w r1, i 9)i.r2'
  ${If} $2 == 0
    MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Impossible de mettre à jour ce fichier :$\r$\n$1$\r$\nFermez PRISME et votre logiciel audio, puis réessayez." /SD IDCANCEL IDRETRY retry
    Delete "$0"
    SetErrorLevel 1
    Abort
  ${EndIf}
FunctionEnd

Section "Uninstall"
  SetRegView 64
  SetShellVarContext all
  Delete "$COMMONFILES64\VST3\Prisme.vst3\Contents\x86_64-win\Prisme.vst3"
  Delete "$COMMONFILES64\VST3\Prisme.vst3\Contents\Resources\moduleinfo.json"
  RMDir "$COMMONFILES64\VST3\Prisme.vst3\Contents\Resources"
  RMDir "$COMMONFILES64\VST3\Prisme.vst3\Contents\x86_64-win"
  RMDir "$COMMONFILES64\VST3\Prisme.vst3\Contents"
  RMDir "$COMMONFILES64\VST3\Prisme.vst3"
  ReadRegDWORD $0 HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AtelierPrismeVST3" "Standalone"
  ${If} $0 == 1
    Delete "$INSTDIR\Prisme.exe"
    Delete "$SMPROGRAMS\Atelier Prisme\Prisme\PRISME.lnk"
    RMDir "$SMPROGRAMS\Atelier Prisme\Prisme"
    RMDir "$SMPROGRAMS\Atelier Prisme"
  ${EndIf}
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\AtelierPrismeVST3"
  Delete "$INSTDIR\Desinstaller-PRISME.exe"
  RMDir "$INSTDIR"
SectionEnd
