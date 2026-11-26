; Per-user installer for pvr2png and the Explorer thumbnail provider and preview handler.
;
; makensis -DVERSION=1.0.0 -DARCH=x64|arm64 -DSTAGING=<install prefix> -DSOURCE_DIR=<checkout>
;   -DOUTFILE=<setup.exe> quickpvr.nsi

Unicode true
ManifestDPIAware true
SetCompressor /SOLID lzma

!include 'LogicLib.nsh'
!include 'MUI2.nsh'
!include 'x64.nsh'

!ifndef VERSION
  !error 'VERSION is not defined.'
!endif
!ifndef ARCH
  !error 'ARCH is not defined.'
!endif
!ifndef STAGING
  !error 'STAGING is not defined.'
!endif
!ifndef SOURCE_DIR
  !error 'SOURCE_DIR is not defined.'
!endif
!ifndef OUTFILE
  !error 'OUTFILE is not defined.'
!endif

!define PRODUCT 'QuickPVR'
!define PUBLISHER 'Andrew Udvare'
!define WEBSITE 'https://github.com/Tatsh/quickpvr'
!define SETTINGS_KEY 'Software\QuickPVR'
!define UNINSTALL_KEY 'Software\Microsoft\Windows\CurrentVersion\Uninstall\QuickPVR'
!define SHELL_DLL 'QuickPVRShell.dll'
; SHCNE_ASSOCCHANGED tells Explorer to drop its cached handlers.
!define SHCNE_ASSOCCHANGED 0x08000000

Name '${PRODUCT}'
OutFile '${OUTFILE}'
RequestExecutionLevel user
InstallDir '$LOCALAPPDATA\Programs\QuickPVR'
InstallDirRegKey HKCU '${SETTINGS_KEY}' 'InstallDir'
BrandingText '${PRODUCT} ${VERSION}'

VIProductVersion '${VERSION}.0'
VIAddVersionKey 'CompanyName' '${PUBLISHER}'
VIAddVersionKey 'FileDescription' '${PRODUCT} Setup'
VIAddVersionKey 'FileVersion' '${VERSION}'
VIAddVersionKey 'LegalCopyright' 'Copyright (c) ${PUBLISHER}'
VIAddVersionKey 'ProductName' '${PRODUCT}'
VIAddVersionKey 'ProductVersion' '${VERSION}'

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_LICENSE '${SOURCE_DIR}\LICENSE.txt'
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE 'English'

; The shell extension is native code, so it only loads on the architecture it was built for.
!macro CheckArchitecture
  !if '${ARCH}' == 'arm64'
    ${IfNot} ${IsNativeARM64}
      MessageBox MB_ICONSTOP|MB_OK 'This installer is for Windows on Arm.' /SD IDOK
      Abort
    ${EndIf}
  !else
    ${IfNot} ${IsNativeAMD64}
      MessageBox MB_ICONSTOP|MB_OK 'This installer is for 64-bit Windows on x64.' /SD IDOK
      Abort
    ${EndIf}
  !endif
!macroend

; The installer is a 32-bit process, so the native regsvr32 is reached with file system
; redirection off. It loads the DLL in a process of the right architecture.
!macro RunRegsvr32 Arguments Result
  ${DisableX64FSRedirection}
  ExecWait '"$SYSDIR\regsvr32.exe" /s ${Arguments} "$INSTDIR\${SHELL_DLL}"' ${Result}
  ${EnableX64FSRedirection}
!macroend

Function .onInit
  !insertmacro CheckArchitecture
  SetRegView 64
FunctionEnd

Function un.onInit
  SetRegView 64
FunctionEnd

Section 'QuickPVR'
  SetOutPath '$INSTDIR'
  ; A handler Explorer still has loaded cannot be overwritten, but it can be renamed out of the
  ; way. Copies renamed by an earlier installation are removed once nothing has them loaded.
  Delete '$INSTDIR\${SHELL_DLL}.old*'
  ${If} ${FileExists} '$INSTDIR\${SHELL_DLL}'
    ClearErrors
    Delete '$INSTDIR\${SHELL_DLL}'
    ${If} ${Errors}
      System::Call 'kernel32::GetTickCount() i .r0'
      Rename '$INSTDIR\${SHELL_DLL}' '$INSTDIR\${SHELL_DLL}.old.$0'
    ${EndIf}
  ${EndIf}

  File '${STAGING}\bin\${SHELL_DLL}'
  File '${STAGING}\bin\pvr2png.exe'
  File '${SOURCE_DIR}\LICENSE.txt'
  WriteUninstaller '$INSTDIR\uninstall.exe'

  !insertmacro RunRegsvr32 '' $0
  ${If} $0 != 0
    MessageBox MB_ICONSTOP|MB_OK 'Registering ${SHELL_DLL} failed (regsvr32 exited with $0).' \
      /SD IDOK
    SetErrorLevel 2
    Quit
  ${EndIf}

  WriteRegStr HKCU '${SETTINGS_KEY}' 'InstallDir' '$INSTDIR'
  WriteRegStr HKCU '${UNINSTALL_KEY}' 'DisplayName' '${PRODUCT}'
  WriteRegStr HKCU '${UNINSTALL_KEY}' 'DisplayVersion' '${VERSION}'
  WriteRegStr HKCU '${UNINSTALL_KEY}' 'DisplayIcon' '$INSTDIR\uninstall.exe'
  WriteRegStr HKCU '${UNINSTALL_KEY}' 'InstallLocation' '$INSTDIR'
  WriteRegStr HKCU '${UNINSTALL_KEY}' 'Publisher' '${PUBLISHER}'
  WriteRegStr HKCU '${UNINSTALL_KEY}' 'URLInfoAbout' '${WEBSITE}'
  WriteRegStr HKCU '${UNINSTALL_KEY}' 'UninstallString' '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKCU '${UNINSTALL_KEY}' 'QuietUninstallString' '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKCU '${UNINSTALL_KEY}' 'NoModify' 1
  WriteRegDWORD HKCU '${UNINSTALL_KEY}' 'NoRepair' 1

  System::Call 'shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i 0, p 0, p 0)'
SectionEnd

Section 'Uninstall'
  ${If} ${FileExists} '$INSTDIR\${SHELL_DLL}'
    !insertmacro RunRegsvr32 '/u' $0
  ${EndIf}
  System::Call 'shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i 0, p 0, p 0)'

  Delete '$INSTDIR\${SHELL_DLL}'
  Delete '$INSTDIR\${SHELL_DLL}.old*'
  Delete '$INSTDIR\pvr2png.exe'
  Delete '$INSTDIR\LICENSE.txt'
  Delete '$INSTDIR\uninstall.exe'
  RMDir '$INSTDIR'

  DeleteRegKey HKCU '${UNINSTALL_KEY}'
  DeleteRegKey HKCU '${SETTINGS_KEY}'
SectionEnd
