; SoilWaterCM Windows Installer Script (NSIS)
; Requires NSIS 3.0+

!include "MUI2.nsh"
!include "FileFunc.nsh"

; Application info
Name "SoilWaterCM C++ Backend"
OutFile "SoilWaterCM-Setup.exe"
InstallDir "$PROGRAMFILES64\SoilWaterCM"
InstallDirRegKey HKLM "Software\SoilWaterCM" "Install_Dir"
RequestExecutionLevel admin

; Interface settings
!define MUI_ABORTWARNING
!define MUI_ICON "resources\icon.ico"
!define MUI_UNICON "resources\icon.ico"

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "SimpChinese"

; Installer sections
Section "SoilWaterCM Backend (Required)" SecMain
    SectionIn RO
    
    SetOutPath $INSTDIR
    
    ; Copy files
    File /r "bin\*.*"
    File /r "config\*.*"
    File /r "docs\*.*"
    
    ; Create data directories
    CreateDirectory "$INSTDIR\data"
    CreateDirectory "$INSTDIR\data\images"
    CreateDirectory "$INSTDIR\data\results"
    CreateDirectory "$INSTDIR\data\reports"
    CreateDirectory "$INSTDIR\data\temp"
    CreateDirectory "$INSTDIR\logs"
    
    ; Write installation info
    WriteRegStr HKLM "Software\SoilWaterCM" "Install_Dir" "$INSTDIR"
    WriteRegStr HKLM "Software\SoilWaterCM" "Version" "1.0.0"
    
    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; Add to PATH
    EnVar::AddValue "PATH" "$INSTDIR\bin"
    
SectionEnd

Section "Visual C++ Redistributable" SecVCRedist
    ; Install VC++ redistributable if needed
    IfFileExists "$SYSDIR\vcruntime140.dll" +3 0
    File "resources\vcredist_x64.exe"
    ExecWait '"$INSTDIR\vcredist_x64.exe" /quiet /norestart'
SectionEnd

Section "Start Menu Shortcuts" SecShortcuts
    CreateDirectory "$SMPROGRAMS\SoilWaterCM"
    CreateShortcut "$SMPROGRAMS\SoilWaterCM\SoilWaterCM.lnk" "$INSTDIR\bin\soilwatercm.exe"
    CreateShortcut "$SMPROGRAMS\SoilWaterCM\Configuration.lnk" "$INSTDIR\config"
    CreateShortcut "$SMPROGRAMS\SoilWaterCM\Documentation.lnk" "$INSTDIR\docs"
    CreateShortcut "$SMPROGRAMS\SoilWaterCM\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Desktop Shortcut" SecDesktop
    CreateShortcut "$DESKTOP\SoilWaterCM.lnk" "$INSTDIR\bin\soilwatercm.exe"
SectionEnd

Section "Windows Service" SecService
    ; Install as Windows service using NSSM
    File "resources\nssm.exe"
    ExecWait '"$INSTDIR\nssm.exe" install SoilWaterCM "$INSTDIR\bin\soilwatercm.exe"'
    ExecWait '"$INSTDIR\nssm.exe" set SoilWaterCM AppDirectory "$INSTDIR"'
    ExecWait '"$INSTDIR\nssm.exe" set SoilWaterCM DisplayName "SoilWaterCM C++ Backend"'
    ExecWait '"$INSTDIR\nssm.exe" set SoilWaterCM Description "SoilWaterCM C++ Backend Service"'
    ExecWait '"$INSTDIR\nssm.exe" set SoilWaterCM Start SERVICE_AUTO_START'
SectionEnd

; Section descriptions
LangString DESC_SecMain ${LANG_ENGLISH} "Main application files (Required)"
LangString DESC_SecMain ${LANG_SIMPCHINESE} "主程序文件（必需）"
LangString DESC_SecVCRedist ${LANG_ENGLISH} "Visual C++ Redistributable Package"
LangString DESC_SecVCRedist ${LANG_SIMPCHINESE} "Visual C++ 运行库"
LangString DESC_SecShortcuts ${LANG_ENGLISH} "Start Menu Shortcuts"
LangString DESC_SecShortcuts ${LANG_SIMPCHINESE} "开始菜单快捷方式"
LangString DESC_SecDesktop ${LANG_ENGLISH} "Desktop Shortcut"
LangString DESC_SecDesktop ${LANG_SIMPCHINESE} "桌面快捷方式"
LangString DESC_SecService ${LANG_ENGLISH} "Install as Windows Service"
LangString DESC_SecService ${LANG_SIMPCHINESE} "安装为 Windows 服务"

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} $(DESC_SecMain)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecVCRedist} $(DESC_SecVCRedist)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecShortcuts} $(DESC_SecShortcuts)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} $(DESC_SecDesktop)
    !insertmacro MUI_DESCRIPTION_TEXT ${SecService} $(DESC_SecService)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; Uninstaller
Section "Uninstall"
    ; Stop service if installed
    ReadRegStr $0 HKLM "System\CurrentControlSet\Services\SoilWaterCM" "ImagePath"
    StrCmp $0 "" +3 0
    ExecWait '"$INSTDIR\nssm.exe" stop SoilWaterCM'
    ExecWait '"$INSTDIR\nssm.exe" remove SoilWaterCM confirm'
    
    ; Remove files
    RMDir /r "$INSTDIR\bin"
    RMDir /r "$INSTDIR\config"
    RMDir /r "$INSTDIR\docs"
    RMDir /r "$INSTDIR\data"
    RMDir /r "$INSTDIR\logs"
    Delete "$INSTDIR\nssm.exe"
    Delete "$INSTDIR\vcredist_x64.exe"
    Delete "$INSTDIR\Uninstall.exe"
    RMDir "$INSTDIR"
    
    ; Remove shortcuts
    Delete "$SMPROGRAMS\SoilWaterCM\SoilWaterCM.lnk"
    Delete "$SMPROGRAMS\SoilWaterCM\Configuration.lnk"
    Delete "$SMPROGRAMS\SoilWaterCM\Documentation.lnk"
    Delete "$SMPROGRAMS\SoilWaterCM\Uninstall.lnk"
    RMDir "$SMPROGRAMS\SoilWaterCM"
    Delete "$DESKTOP\SoilWaterCM.lnk"
    
    ; Remove from PATH
    EnVar::DeleteValue "PATH" "$INSTDIR\bin"
    
    ; Remove registry keys
    DeleteRegKey HKLM "Software\SoilWaterCM"
    DeleteRegKey /ifempty HKLM "Software\SoilWaterCM"
SectionEnd

; Functions
Function .onInit
    ; Check if already installed
    ReadRegStr $0 HKLM "Software\SoilWaterCM" "Install_Dir"
    StrCmp $0 "" +4 0
    MessageBox MB_YESNO|MB_ICONQUESTION \
        "SoilWaterCM is already installed in $0.$\n$\nWould you like to uninstall it first?" \
        /SD IDYES IDNO +2
    ExecWait '"$0\Uninstall.exe" /S _?=$0'
FunctionEnd
