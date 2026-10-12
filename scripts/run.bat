@echo off
REM SoilWaterCM C++ Backend Launcher (Batch)
REM 自动设置 PATH 并启动后端服务

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%..\

REM 构建配置：Release / RelWithDebInfo / Debug
if "%~1"=="" ( set BUILD_CONFIG=Release ) else ( set BUILD_CONFIG=%~1 )

REM 可执行文件路径
set EXE_PATH=%PROJECT_ROOT%build\%BUILD_CONFIG%\soilwatercm_app.exe
if not exist "%EXE_PATH%" (
    echo [ERROR] Executable not found: %EXE_PATH%
    echo Usage: scripts\run.bat [Release^|RelWithDebInfo^|Debug]
    pause
    exit /b 1
)

REM 收集 DLL 目录
set DLL_DIRS=%PROJECT_ROOT%vcpkg_installed\x64-windows\bin;%PROJECT_ROOT%thirdparty\vendor\osgeo4w\bin;%PROJECT_ROOT%thirdparty\vendor\osgeo4w\apps\qgis-ltr\bin

REM 设置环境变量
set PATH=%DLL_DIRS%;%PATH%

REM 设置 QGIS/GDAL 环境变量
set QGIS_PREFIX_PATH=%PROJECT_ROOT%thirdparty\vendor\osgeo4w
set GDAL_DATA=%PROJECT_ROOT%thirdparty\vendor\osgeo4w\share\gdal
set PROJ_LIB=%PROJECT_ROOT%thirdparty\vendor\osgeo4w\share\proj
set GDAL_DRIVER_PATH=%PROJECT_ROOT%thirdparty\vendor\osgeo4w\apps\gdal\lib\gdalplugins

echo ========================================
echo  SoilWaterCM C++ Backend Launcher
echo ========================================
echo Config: %BUILD_CONFIG%
echo Exe:    %EXE_PATH%
echo.

echo Starting server...
echo.

cd /d "%PROJECT_ROOT%"
"%EXE_PATH%"

if %ERRORLEVEL% NEQ 0 (
    echo Server exited with code: %ERRORLEVEL%
)

pause
