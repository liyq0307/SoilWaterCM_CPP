# SoilWaterCM C++ Backend Launcher (PowerShell)
# 自动设置 PATH 并启动后端服务

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir

# 构建配置：Release / RelWithDebInfo / Debug
$BuildConfig = if ($args[0]) { $args[0] } else { "Release" }

# 可执行文件路径
$ExePath = Join-Path $ProjectRoot "build" $BuildConfig "soilwatercm_app.exe"
if (-not (Test-Path $ExePath)) {
    Write-Error "Executable not found: $ExePath"
    Write-Host "Usage: .\scripts\run.ps1 [Release|RelWithDebInfo|Debug]" -ForegroundColor Yellow
    exit 1
}

# 收集所有需要加入 PATH 的 DLL 目录
$DllDirs = @(
    # vcpkg 安装的 Release DLL
    Join-Path $ProjectRoot "vcpkg_installed\x64-windows\bin"
    # OSGeo4W 的运行时 DLL（gdal313.dll, proj_9.dll, geos_c.dll 等）
    Join-Path $ProjectRoot "thirdparty\vendor\osgeo4w\bin"
    # 额外的一些子目录
    Join-Path $ProjectRoot "thirdparty\vendor\osgeo4w\apps\qgis-ltr\bin"
)

# 过滤出实际存在的目录
$ValidDirs = $DllDirs | Where-Object { Test-Path $_ }

# 设置环境变量（保留原有 PATH）
$env:Path = ($ValidDirs -join [System.IO.Path]::PathSeparator) + [System.IO.Path]::PathSeparator + $env:Path

# 设置 QGIS 相关环境变量
$env:QGIS_PREFIX_PATH = Join-Path $ProjectRoot "thirdparty\vendor\osgeo4w"
$env:GDAL_DATA = Join-Path $ProjectRoot "thirdparty\vendor\osgeo4w\share\gdal"
$env:PROJ_LIB = Join-Path $ProjectRoot "thirdparty\vendor\osgeo4w\share\proj"
$env:GDAL_DRIVER_PATH = Join-Path $ProjectRoot "thirdparty\vendor\osgeo4w\apps\gdal\lib\gdalplugins"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " SoilWaterCM C++ Backend Launcher" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Config : $BuildConfig"
Write-Host "Exe    : $ExePath"
Write-Host "DLL    : $($ValidDirs -join ', ')"
Write-Host ""
Write-Host "Starting server..." -ForegroundColor Green
Write-Host ""

# 切换到项目根目录（config.yaml 等配置文件的相对路径基于此）
Push-Location $ProjectRoot
try {
    & $ExePath @args[1..$args.Length]
    $exitCode = $LASTEXITCODE
} finally {
    Pop-Location
}

if ($exitCode -and $exitCode -ne 0) {
    Write-Host "Server exited with code: $exitCode" -ForegroundColor Red
}
exit $exitCode
