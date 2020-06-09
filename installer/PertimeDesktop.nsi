; pertime nsis script
!define PRODUCT_NAME "PertimeDesktop"
!define PRODUCT_VERSION "0.1.0"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

SetCompressor lzma
;!define MUI_ABORTWARNING
;!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
;!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"
;!insertmacro MUI_LANGUAGE "SimpChinese"
;!insertmacro MUI_LANGUAGE "English"

; The name of the installer
Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"

; The file to write
OutFile "PertimeDesktop-0.1.0-setup.exe"
Unicode True
;Request UAC elevation
RequestExecutionLevel admin 
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"
;Page components
Page directory
Page instfiles

Uninstpage uninstConfirm
Uninstpage instfiles

ShowInstDetails show
ShowUnInstDetails show
BrandingText " "

; Registry key to check for directory (so if you install again, it will 
; overwrite the old one automatically)
;InstallDirRegKey HKLM "Software\ieh.me" "Install_Dir"

;Var AppDataPath
;Var ShotsPath

;--------------------------------
Section "MainSection" SEC01
  SetOutPath "$INSTDIR\*.*"
  SetOverwrite ifnewer
  WriteUninstaller "$InstDir\Uninst.exe"
  File /r "files\*.*"
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

