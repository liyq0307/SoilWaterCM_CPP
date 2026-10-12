# SoilWaterCM C++ Backend 构建指南

## 快速开始

### Windows

```powershell
# 1. 配置项目（自动下载vcpkg和所有依赖）
cmake -B build -S . -G "Visual Studio 16 2019" -A x64

# 2. 编译
cmake --build build --config Release

# 3. 运行测试（可选）
cd build
ctest -C Release --output-on-failure
```

### Linux/macOS

```bash
# 1. 配置项目（自动下载vcpkg和所有依赖）
cmake -B build -S .

# 2. 编译
cmake --build build -j$(nproc)

# 3. 运行测试（可选）
cd build
ctest --output-on-failure
```

## 构建选项

```bash
# Debug模式
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# 启用C++20
cmake -B build -S . -DENABLE_CPP20=ON

# 禁用测试
cmake -B build -S . -DBUILD_TESTS=OFF

# 指定安装路径
cmake -B build -S . -DCMAKE_INSTALL_PREFIX=/path/to/install
```

## 清理构建

```bash
# 删除build目录重新构建
rm -rf build
cmake -B build -S .

# 或使用cmake的--fresh选项（CMake 3.24+）
cmake -B build -S . --fresh
```

## 依赖说明

### 自动安装的依赖（通过vcpkg）

项目会在首次cmake配置时自动下载安装：

- **Web框架**: drogon, nlohmann-json, spdlog
- **数据库**: pqxx (PostgreSQL), mongocxx (MongoDB), hiredis (Redis)
- **认证加密**: jwt-cpp, openssl
- **工具库**: boost-filesystem, boost-system, boost-thread, yaml-cpp
- **测试**: gtest, benchmark

### 手动配置的环境（thirdparty/vendor目录）

QGIS等地理信息工具需手动安装到`thirdparty/vendor`目录：

```
thirdparty/vendor/
├── osgeo4w/     # QGIS + GDAL + GRASS + SAGA
├── otb/         # Orfeo ToolBox (可选)
└── snap/        # Sentinel Application Platform (可选)
```

参考 `thirdparty/vendor/README.md` 安装说明。

## 目录结构

```
SoilWaterCM_CPP/
├── build/              # 构建输出（自动生成）
├── cmake/              # CMake模块
│   ├── VcpkgSetup.cmake    # vcpkg自动配置
│   ├── FindQGIS.cmake      # QGIS查找
│   └── EnvConfig.cmake     # 环境配置
├── src/                # 源代码
├── tests/              # 测试代码
├── thirdparty/         # 第三方工具
│   ├── vcpkg/          # vcpkg（自动下载）
│   └── vendor/         # 外部环境（不入git）
│       ├── osgeo4w/
│       ├── otb/
│       └── snap/
└── vcpkg.json          # vcpkg依赖清单
```

## 常见问题

### Q1: vcpkg下载慢怎么办？

设置代理：
```bash
export VCPKG_BINARY_SOURCES="clear;x-azurl,https://vcpkg-binary-cache.example.com/,readwrite"
```

### Q2: 找不到QGIS/GDAL？

确保QGIS安装在`thirdparty/vendor/osgeo4w`目录，或设置环境变量：
```bash
set QGIS_PREFIX_PATH=E:\WorkSpace\SoilWaterCM_CPP\thirdparty\vendor\osgeo4w
```

### Q3: 编译错误？

1. 确保Visual Studio 2019+已安装
2. 确保git已安装（vcpkg需要）
3. 删除build目录重新构建

## 安装

```bash
cmake --install build --prefix ./install
```

可执行文件将安装到 `install/bin/`
