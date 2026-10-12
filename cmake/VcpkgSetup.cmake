# VcpkgSetup.cmake - 自动下载安装vcpkg及其依赖
#
# 此模块在cmake配置时自动：
#   1. 下载并安装vcpkg（如果不存在）
#   2. 安装vcpkg.json中定义的所有依赖
#
# 用法：
#   include(VcpkgSetup)

cmake_minimum_required(VERSION 3.16)

message(STATUS "")
message(STATUS "=== Setting up vcpkg and dependencies ===")
message(STATUS "")

# 确定vcpkg安装位置
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT "$ENV{VCPKG_ROOT}")
else()
    set(VCPKG_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/vcpkg")
endif()

message(STATUS "vcpkg location: ${VCPKG_ROOT}")

# 确定triplet和可执行文件
if(WIN32)
    set(TRIPLET "x64-windows")
    set(VCPKG_EXE "${VCPKG_ROOT}/vcpkg.exe")
else()
    if(APPLE)
        set(TRIPLET "x64-osx")
    else()
        set(TRIPLET "x64-linux")
    endif()
    set(VCPKG_EXE "${VCPKG_ROOT}/vcpkg")
endif()

# 步骤1：检查并安装vcpkg
if(NOT EXISTS "${VCPKG_ROOT}")
    # vcpkg目录不存在，需要clone
    message(STATUS "vcpkg directory not found, cloning...")
    
    # 检查git
    find_program(GIT_EXE git)
    if(NOT GIT_EXE)
        message(FATAL_ERROR "git not found! Please install git first.")
    endif()
    
    # 确保thirdparty目录存在
    get_filename_component(VCPKG_PARENT "${VCPKG_ROOT}" DIRECTORY)
    if(NOT EXISTS "${VCPKG_PARENT}")
        file(MAKE_DIRECTORY "${VCPKG_PARENT}")
    endif()
    
    # 克隆vcpkg
    execute_process(
        COMMAND ${GIT_EXE} clone https://github.com/microsoft/vcpkg.git
        WORKING_DIRECTORY "${VCPKG_PARENT}"
        RESULT_VARIABLE CLONE_RESULT
        OUTPUT_VARIABLE CLONE_OUTPUT
        ERROR_VARIABLE CLONE_ERROR
    )
    
    if(NOT CLONE_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to clone vcpkg:\n${CLONE_ERROR}")
    endif()
    
    message(STATUS "vcpkg cloned successfully")
endif()

# 检查vcpkg可执行文件
if(NOT EXISTS "${VCPKG_EXE}")
    # vcpkg目录存在但可执行文件不存在，需要bootstrap
    message(STATUS "vcpkg executable not found, bootstrapping...")
    
    if(WIN32)
        execute_process(
            COMMAND cmd /C bootstrap-vcpkg.bat
            WORKING_DIRECTORY "${VCPKG_ROOT}"
            RESULT_VARIABLE BOOTSTRAP_RESULT
            OUTPUT_VARIABLE BOOTSTRAP_OUTPUT
            ERROR_VARIABLE BOOTSTRAP_ERROR
        )
    else()
        execute_process(
            COMMAND ./bootstrap-vcpkg.sh
            WORKING_DIRECTORY "${VCPKG_ROOT}"
            RESULT_VARIABLE BOOTSTRAP_RESULT
            OUTPUT_VARIABLE BOOTSTRAP_OUTPUT
            ERROR_VARIABLE BOOTSTRAP_ERROR
        )
    endif()
    
    if(NOT BOOTSTRAP_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to bootstrap vcpkg:\n${BOOTSTRAP_ERROR}")
    endif()
    
    message(STATUS "vcpkg bootstrapped successfully")
else()
    message(STATUS "vcpkg already installed: ${VCPKG_EXE}")
endif()

# 步骤2：安装vcpkg.json中的依赖
set(VCPKG_INSTALL_SENTINEL "${VCPKG_ROOT}/packages/.vcpkg_installed_${TRIPLET}")
if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg.json" AND NOT EXISTS "${VCPKG_INSTALL_SENTINEL}")
    message(STATUS "")
    message(STATUS "Installing dependencies from vcpkg.json...")
    message(STATUS "Triplet: ${TRIPLET}")
    message(STATUS "")
    
    execute_process(
        COMMAND "${VCPKG_EXE}" install --triplet=${TRIPLET}
        WORKING_DIRECTORY "${VCPKG_ROOT}"
        RESULT_VARIABLE INSTALL_RESULT
        OUTPUT_VARIABLE INSTALL_OUTPUT
        ERROR_VARIABLE INSTALL_ERROR
        COMMAND_ECHO STDOUT
        ECHO_OUTPUT_VARIABLE
        ECHO_ERROR_VARIABLE
    )
    
    if(NOT INSTALL_RESULT EQUAL 0)
        message("")
        message(FATAL_ERROR "Failed to install vcpkg dependencies (exit code ${INSTALL_RESULT}):\n${INSTALL_ERROR}\n\nOutput:\n${INSTALL_OUTPUT}")
    endif()
    
    file(TOUCH "${VCPKG_INSTALL_SENTINEL}")
    message(STATUS "")
    message(STATUS "All vcpkg dependencies installed successfully")
elseif(EXISTS "${VCPKG_INSTALL_SENTINEL}")
    message(STATUS "")
    message(STATUS "vcpkg dependencies already installed (sentinel: ${VCPKG_INSTALL_SENTINEL})")
else()
    message(WARNING "vcpkg.json not found, skipping dependency installation")
endif()

# 依赖已就绪时关闭 toolchain 的隐式 manifest 安装:
# 否则每次 cmake 重新配置都会重跑 vcpkg install (权限受限环境下会失败, 导致工程反复重配置失败)
# 新增依赖后删除上述 sentinel 文件即可恢复自动安装
if(EXISTS "${VCPKG_INSTALL_SENTINEL}")
    set(VCPKG_MANIFEST_INSTALL OFF CACHE BOOL "Skip implicit vcpkg install once deps are present" FORCE)
endif()

# 步骤3：设置CMAKE_TOOLCHAIN_FILE
set(VCPKG_TOOLCHAIN "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
if(NOT EXISTS "${VCPKG_TOOLCHAIN}")
    message(FATAL_ERROR "vcpkg toolchain file not found: ${VCPKG_TOOLCHAIN}")
endif()

set(CMAKE_TOOLCHAIN_FILE "${VCPKG_TOOLCHAIN}" CACHE PATH "vcpkg toolchain file" FORCE)

message(STATUS "")
message(STATUS "=== vcpkg setup complete ===")
message(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
message(STATUS "")
