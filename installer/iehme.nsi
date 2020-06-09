; ieh nsis script
!define PRODUCT_NAME "PertimeDesktop"
!define PRODUCT_VERSION "0.1.0"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

SetCompressor lzma
;--------------------------------
!include "MUI2.nsh"
; MUI vars
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
; 欢迎页面
!insertmacro MUI_PAGE_WELCOME
; 安装目录选择页面
!insertmacro MUI_PAGE_DIRECTORY
; 安装过程页面
!insertmacro MUI_PAGE_INSTFILES
; 安装完成页面
!insertmacro MUI_PAGE_FINISH
; 安装卸载过程页面
!insertmacro MUI_UNPAGE_INSTFILES
; 安装界面包含的语言设置
;!insertmacro MUI_LANGUAGE "SimpChinese"
!insertmacro MUI_LANGUAGE "English"
; ------ MUI 现代界面定义结束 ------

; The name of the installer
Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"

; The file to write
OutFile "PertimeDesktop-0.1.0-setup.exe"

; The default installation directory
InstallDir $PROGRAMFILES32\ieh.me
ShowInstDetails show
ShowUnInstDetails show
BrandingText " "

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
;InstallDirRegKey HKLM "Software\ieh.me" "Install_Dir"

; Request application privileges for Windows Vista
RequestExecutionLevel admin
Var AppDataPath
Var ShotsPath

;--------------------------------
Section "MainSection" SEC01
  SetOutPath "$INSTDIR\*.*"
  SetOverwrite ifnewer
  File /r "files\*.*"
SectionEnd

Section -Post
  ; Install a service - ServiceType interact with desktop - StartType automatic - Dependencies on "Windows Time Service" (w32time) and "WWW Publishing Service" (w3svc) - Logon as System Account
  ; 272 - LocalSystem interactive
  ; 2 - auto-start
  SimpleSC::InstallService "timeout" "timeout" "272" "2" "$INSTDIR\timeout.exe" "" "" ""
  Pop $0 ; returns an errorcode (<>0) otherwise success (0)
  SectionIn RO
  ReadEnvStr $0 "ALLUSERSPROFILE"
  DetailPrint %ALLUSERSPROFILE%=$0
  StrCpy $AppDataPath "$0\TimePie"
  CreateDirectory "$AppDataPath"
  StrCpy $ShotsPath "$0\TimePie\shots"
  CreateDirectory "$ShotsPath"
  SetShellVarContext all
  ;AccessControl::GrantOnFile "$AppDataPath" "(S-1-5-32-545)" "FullAccess"
  AccessControl::GrantOnFile "$AppDataPath" "(BU)" "GenericRead + GenericWrite"
  WriteUninstaller "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayName" "$(^Name)"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\uninst.exe"
  WriteRegStr ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
SectionEnd

;--------------------------------
; Uninstaller
Section Uninstall
  Delete "$INSTDIR\uninst.exe"


  RMDir /r "$INSTDIR\*.*"

  RMDir "$INSTDIR"

  DeleteRegKey ${PRODUCT_UNINST_ROOT_KEY} "${PRODUCT_UNINST_KEY}"
  SetAutoClose true
SectionEnd


