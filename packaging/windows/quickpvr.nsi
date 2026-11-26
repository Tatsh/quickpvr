; Installer for pvr2png and the Explorer thumbnail provider, preview handler, and property handler.
; It installs for every user (with administrator rights) or for the current user only. Windows
; loads property handlers only from HKLM. A current-user installation does not add details.
;
; makensis -DVERSION=1.0.0 -DARCH=x64|arm64 -DSTAGING=<install prefix> -DSOURCE_DIR=<checkout>
;   -DOUTFILE=<setup.exe> quickpvr.nsi
;
; Silent installations take /S with /AllUsers or /CurrentUser.

Unicode true
ManifestDPIAware true
SetCompressor /SOLID lzma

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
!define PUBLISHER 'Tatsh'
!define COPYRIGHT 'Copyright (c) 2009-2010 Limbic Software, Inc. Copyright (c) 2018-2026 Andrew \
Udvare. BSD 3-Clause License.'
!define WEBSITE 'https://github.com/Tatsh/quickpvr'
!define SETTINGS_KEY 'Software\QuickPVR'
!define UNINSTALL_KEY 'Software\Microsoft\Windows\CurrentVersion\Uninstall\QuickPVR'
!define SHELL_DLL 'QuickPVRShell.dll'
; SHCNE_ASSOCCHANGED tells Explorer to drop its cached handlers.
!define SHCNE_ASSOCCHANGED 0x08000000

!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_USE_PROGRAMFILES64
!define MULTIUSER_INSTALLMODE_INSTDIR '${PRODUCT}'
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_KEY '${SETTINGS_KEY}'
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_VALUENAME 'InstallDir'
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_KEY '${SETTINGS_KEY}'
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_VALUENAME 'InstallDir'

!include 'LogicLib.nsh'
!include 'MultiUser.nsh'
!include 'MUI2.nsh'
!include 'x64.nsh'

Name '${PRODUCT}'
OutFile '${OUTFILE}'
BrandingText '${PRODUCT} ${VERSION}'

VIProductVersion '${VERSION}.0'
VIFileVersion '${VERSION}.0'
VIAddVersionKey 'Comments' '${WEBSITE}'
VIAddVersionKey 'CompanyName' '${PUBLISHER}'
VIAddVersionKey 'FileDescription' '${PRODUCT} installer'
VIAddVersionKey 'FileVersion' '${VERSION}'
VIAddVersionKey 'InternalName' 'QuickPVR-Setup'
VIAddVersionKey 'LegalCopyright' '${COPYRIGHT}'
VIAddVersionKey 'OriginalFilename' 'QuickPVR-Setup.exe'
VIAddVersionKey 'ProductName' '${PRODUCT}'
VIAddVersionKey 'ProductVersion' '${VERSION}'

!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_LICENSE '${SOURCE_DIR}\LICENSE.txt'
!insertmacro MULTIUSER_PAGE_INSTALLMODE
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

; The installer is a 32-bit process. File system redirection is turned off to run the native
; regsvr32. It loads the DLL in a process of the right architecture. DllInstall registers
; for every user with /i:machine and for the current user with /i:user.
!macro RunRegsvr32 Arguments Result
  ${If} $MultiUser.InstallMode == 'AllUsers'
    StrCpy $1 'machine'
  ${Else}
    StrCpy $1 'user'
  ${EndIf}
  ${DisableX64FSRedirection}
  ExecWait '"$SYSDIR\regsvr32.exe" /s ${Arguments} /n /i:$1 "$INSTDIR\${SHELL_DLL}"' ${Result}
  ${EnableX64FSRedirection}
!macroend

; The 64-bit registry view places the settings and uninstall entries where 64-bit Windows looks.
Function .onInit
  !insertmacro CheckArchitecture
  SetRegView 64
  !insertmacro MULTIUSER_INIT
FunctionEnd

Function un.onInit
  SetRegView 64
  !insertmacro MULTIUSER_UNINIT
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
  File '${STAGING}\bin\QuickPVR.propdesc'
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

  ; SHCTX is HKLM for every user and HKCU for the current user.
  WriteRegStr SHCTX '${SETTINGS_KEY}' 'InstallDir' '$INSTDIR'
  WriteRegStr SHCTX '${UNINSTALL_KEY}' 'DisplayName' '${PRODUCT}'
  WriteRegStr SHCTX '${UNINSTALL_KEY}' 'DisplayVersion' '${VERSION}'
  WriteRegStr SHCTX '${UNINSTALL_KEY}' 'DisplayIcon' '$INSTDIR\uninstall.exe'
  WriteRegStr SHCTX '${UNINSTALL_KEY}' 'InstallLocation' '$INSTDIR'
  WriteRegStr SHCTX '${UNINSTALL_KEY}' 'Publisher' '${PUBLISHER}'
  WriteRegStr SHCTX '${UNINSTALL_KEY}' 'URLInfoAbout' '${WEBSITE}'
  WriteRegStr SHCTX '${UNINSTALL_KEY}' 'UninstallString' \
    '"$INSTDIR\uninstall.exe" /$MultiUser.InstallMode'
  WriteRegStr SHCTX '${UNINSTALL_KEY}' 'QuietUninstallString' \
    '"$INSTDIR\uninstall.exe" /S /$MultiUser.InstallMode'
  WriteRegDWORD SHCTX '${UNINSTALL_KEY}' 'NoModify' 1
  WriteRegDWORD SHCTX '${UNINSTALL_KEY}' 'NoRepair' 1

  System::Call 'shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i 0, p 0, p 0)'
SectionEnd

Section 'Uninstall'
  ${If} ${FileExists} '$INSTDIR\${SHELL_DLL}'
    !insertmacro RunRegsvr32 '/u' $0
  ${EndIf}
  System::Call 'shell32::SHChangeNotify(i ${SHCNE_ASSOCCHANGED}, i 0, p 0, p 0)'

  Delete '$INSTDIR\${SHELL_DLL}'
  Delete '$INSTDIR\${SHELL_DLL}.old*'
  Delete '$INSTDIR\QuickPVR.propdesc'
  Delete '$INSTDIR\pvr2png.exe'
  Delete '$INSTDIR\LICENSE.txt'
  Delete '$INSTDIR\uninstall.exe'
  RMDir '$INSTDIR'

  DeleteRegKey SHCTX '${UNINSTALL_KEY}'
  DeleteRegKey SHCTX '${SETTINGS_KEY}'
SectionEnd
