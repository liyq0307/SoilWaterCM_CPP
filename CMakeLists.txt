cmake_minimum_required(VERSION 3.16)
project(SoilWaterCM_CPP VERSION 1.0.0 LANGUAGES CXX)

if(NOT CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    # 用户在命令行或 GUI 显式指定了 CMAKE_INSTALL_PREFIX, 尊重它
else()
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}" CACHE PATH "Install prefix" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_CPP20 "Enable C++20 features" OFF)
if(ENABLE_CPP20)
    set(CMAKE_CXX_STANDARD 20)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Setup vcpkg toolchain and auto-install dependencies
include(VcpkgSetup)

include(CompilerOptions)

# Set Boost policy to suppress warnings
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Prefer vcpkg packages over system/conda packages
if(DEFINED CMAKE_TOOLCHAIN_FILE)
    # vcpkg installs to vcpkg_installed in project root
    if(WIN32)
        set(VCPKG_INSTALLED_BIN "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows")
        set(VCPKG_INSTALLED_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/vcpkg/installed/x64-windows")
    elseif(APPLE)
        set(VCPKG_INSTALLED_BIN "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-osx")
        set(VCPKG_INSTALLED_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/vcpkg/installed/x64-osx")
    else()
        set(VCPKG_INSTALLED_BIN "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-linux")
        set(VCPKG_INSTALLED_ROOT "${CMAKE_SOURCE_DIR}/thirdparty/vcpkg/installed/x64-linux")
    endif()

    # vcpkg manifest 模式安装到 binary dir, 哨兵跳过安装时仍可由此找到依赖
    if(EXISTS "${VCPKG_INSTALLED_BIN}")
        list(INSERT CMAKE_PREFIX_PATH 0 "${VCPKG_INSTALLED_BIN}")
        message(STATUS "Preferring vcpkg packages from: ${VCPKG_INSTALLED_BIN}")
    endif()
endif()

# Find QGIS first to use its bundled GDAL/PROJ/GEOS (avoids version conflicts)
find_package(QGIS QUIET)

# Configure external tool environment variables
include(EnvConfig)
setup_env_config()

# GDAL: prefer QGIS-bundled version
if(QGIS_FOUND AND QGIS_GDAL_LIBRARY)
    message(STATUS "Using QGIS-bundled GDAL: ${QGIS_GDAL_LIBRARY}")
    set(GDAL_FOUND TRUE)
    set(GDAL_LIBRARIES "${QGIS_GDAL_LIBRARY}")
    set(GDAL_INCLUDE_DIRS "${QGIS_GDAL_INCLUDE_DIR}")
    set(GDAL_VERSION "${QGIS_GDAL_VERSION}")
    
    # Add QGIS bin dir to PATH for DLL resolution
    if(QGIS_BIN_DIR)
        set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_RPATH};${QGIS_BIN_DIR}")
    endif()
else()
    # Fall back to system GDAL
    find_package(GDAL CONFIG QUIET)
    if(NOT GDAL_FOUND)
        find_package(GDAL 3.8 QUIET)
    endif()
endif()

if(NOT GDAL_FOUND)
    # If QGIS was found but GDAL config/headers are missing, let the user know
    if(QGIS_FOUND)
        message(FATAL_ERROR
            "GDAL development files not found (required >= 3.8).\n"
            "QGIS was found at ${QGIS_PREFIX_PATH}, but it lacks GDAL headers/libs.\n"
            "Options:\n"
            "  - Install GDAL via vcpkg:   vcpkg install gdal\n"
            "  - Install GDAL via conda:   conda install -c conda-forge gdal\n"
            "  - Install GDAL from:        https://gisinternals.com/\n"
            "  - Or set GDAL_DIR to a GDAL CMake config directory"
        )
    else()
        message(FATAL_ERROR
            "GDAL not found (required >= 3.8).\n"
            "Please install QGIS 3.34+ (which includes GDAL) or configure GDAL manually:\n"
            "  - Set GDAL_DIR to the GDAL CMake config directory\n"
            "  - Or set QGIS_HOME to the QGIS installation path"
        )
    endif()
endif()

if(NOT TARGET GDAL::GDAL)
    add_library(GDAL::GDAL UNKNOWN IMPORTED)
    set_target_properties(GDAL::GDAL PROPERTIES
        IMPORTED_LOCATION "${GDAL_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${GDAL_INCLUDE_DIRS}"
    )
endif()

find_package(Boost 1.78 REQUIRED COMPONENTS filesystem system thread)

find_package(OpenSSL REQUIRED)
find_package(utf8proc CONFIG)
find_package(Drogon CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
# mongocxx 的 config 内部依赖 deprecated 的 unofficial-utf8proc, 其 config 文件硬编码了
# message(WARNING "find_package(unofficial-utf8proc) is deprecated ..."), 这里临时屏蔽
set(_SAVED_MSG_LEVEL "${CMAKE_MESSAGE_LOG_LEVEL}")
set(CMAKE_MESSAGE_LOG_LEVEL ERROR)
find_package(mongocxx CONFIG REQUIRED)
set(CMAKE_MESSAGE_LOG_LEVEL "${_SAVED_MSG_LEVEL}")
find_package(hiredis CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)

find_package(OpenCV QUIET)
find_package(Eigen3 QUIET)

# CONFIGURE_DEPENDS: 新增/删除源文件时构建期自动重新 glob, 无需手动重新配置
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "src/*.h")

# Exclude main.cpp from library (each target needs its own main)
list(FILTER SOURCES EXCLUDE REGEX ".*/src/main\\.cpp$")

# IDE 解决方案资源管理器按 src 下的目录层级分组, 且每组内按 Visual Studio 标准细分:
#   <dir>/Header Files  -> 头文件 (.h)
#   <dir>/Source Files  -> 源文件 (.cpp)
# 例: dao/Header Files、dao/Source Files、services/Header Files ...
function(swcm_group_files)
    cmake_parse_arguments(SWCM "" "BASE;PREFIX" "FILES" ${ARGN})
    foreach(_f IN LISTS SWCM_FILES)
        file(RELATIVE_PATH _rel "${SWCM_BASE}" "${_f}")
        get_filename_component(_dir "${_rel}" DIRECTORY)
        get_filename_component(_ext "${_f}" LAST_EXT)
        if(_ext STREQUAL ".h")
            set(_leaf "Header Files")
        else()
            set(_leaf "Source Files")
        endif()
        if(_dir STREQUAL "")
            set(_group "${SWCM_PREFIX}${_leaf}")
        else()
            file(TO_CMAKE_PATH "${_dir}" _dir)
            set(_group "${SWCM_PREFIX}${_dir}/${_leaf}")
        endif()
        string(REPLACE "/" "\\" _group "${_group}")
        source_group("${_group}" FILES "${_f}")
    endforeach()
endfunction()

swcm_group_files(BASE "${CMAKE_SOURCE_DIR}/src" PREFIX "" FILES ${SOURCES} ${HEADERS})

if(MSVC)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
add_library(soilwatercm SHARED ${SOURCES} ${HEADERS})

add_executable(soilwatercm_app src/main.cpp)
source_group("Source Files" FILES "${CMAKE_SOURCE_DIR}/src/main.cpp")

if(MSVC)
    target_compile_options(soilwatercm PRIVATE /std:c++20)
    target_compile_options(soilwatercm_app PRIVATE /std:c++20)
    target_compile_definitions(soilwatercm PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_definitions(soilwatercm_app PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_options(soilwatercm PRIVATE /FORCE:MULTIPLE)
    target_link_options(soilwatercm_app PRIVATE /FORCE:MULTIPLE)
endif()

target_include_directories(soilwatercm PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(soilwatercm_app PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(soilwatercm PUBLIC
    GDAL::GDAL
    Drogon::Drogon
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    libpqxx::pqxx
    mongo::mongocxx_shared
    hiredis::hiredis
    jwt-cpp::jwt-cpp
    Boost::filesystem
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_link_libraries(soilwatercm_app PRIVATE soilwatercm)

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if(CLANG_FORMAT_EXE)
    # 仅提供手动 target; 不在配置期自动格式化 (避免 cmake 重新配置时改动源码)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${SOURCES} ${HEADERS}
        COMMENT "Running clang-format on all source files"
    )
    add_custom_target(check-format
        COMMAND ${CLANG_FORMAT_EXE} --dry-run --Werror -style=file ${SOURCES} ${HEADERS}
        COMMENT "Checking clang-format compliance"
    )
else()
    message(STATUS "clang-format not found — skipping format targets")
endif()

if(OpenCV_FOUND)
    target_link_libraries(soilwatercm PUBLIC ${OpenCV_LIBS})
endif()
if(Eigen3_FOUND)
    target_link_libraries(soilwatercm PUBLIC Eigen3::Eigen)
endif()

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS soilwatercm soilwatercm_app
    RUNTIME DESTINATION bin/$<CONFIG>
)

if(WIN32 AND EXISTS "${VCPKG_INSTALLED_BIN}/bin")
    install(DIRECTORY "${VCPKG_INSTALLED_BIN}/bin/" DESTINATION "bin/Release"
        CONFIGURATIONS Release
        FILES_MATCHING PATTERN "*.dll"
        PATTERN "*gtest*" EXCLUDE PATTERN "*gmock*" EXCLUDE
        PATTERN "*benchmark*" EXCLUDE PATTERN "python*" EXCLUDE
        PATTERN "pkgconf*" EXCLUDE
    )
    foreach(cfg IN ITEMS RelWithDebInfo MinSizeRel Debug)
        install(DIRECTORY "${VCPKG_INSTALLED_BIN}/debug/bin/" DESTINATION "bin/${cfg}"
            CONFIGURATIONS "${cfg}"
            FILES_MATCHING PATTERN "*.dll"
            PATTERN "*gtest*" EXCLUDE PATTERN "*gmock*" EXCLUDE
            PATTERN "*benchmark*" EXCLUDE PATTERN "python*" EXCLUDE
            PATTERN "pkgconf*" EXCLUDE
        )
    endforeach()
endif()

# Install OSGeo4W/QGIS runtime DLLs (gdal, proj, geos, etc.)
if(WIN32 AND QGIS_FOUND AND QGIS_PREFIX_PATH)
    set(QGIS_BIN_DIR "${QGIS_PREFIX_PATH}/bin")
    if(EXISTS "${QGIS_BIN_DIR}/gdal313.dll")
        install(DIRECTORY "${QGIS_BIN_DIR}/" DESTINATION "bin/Release"
            CONFIGURATIONS Release
            FILES_MATCHING PATTERN "*.dll"
            PATTERN "*test*" EXCLUDE PATTERN "*python*" EXCLUDE
            PATTERN "*wx*" EXCLUDE PATTERN "*Qt5*" EXCLUDE
            PATTERN "*qgis*" EXCLUDE PATTERN "*grass*" EXCLUDE
        )
        foreach(cfg IN ITEMS RelWithDebInfo MinSizeRel Debug)
            install(DIRECTORY "${QGIS_BIN_DIR}/" DESTINATION "bin/${cfg}"
                CONFIGURATIONS "${cfg}"
                FILES_MATCHING PATTERN "*.dll"
                PATTERN "*test*" EXCLUDE PATTERN "*python*" EXCLUDE
                PATTERN "*wx*" EXCLUDE PATTERN "*Qt5*" EXCLUDE
                PATTERN "*qgis*" EXCLUDE PATTERN "*grass*" EXCLUDE
            )
        endforeach()
        message(STATUS "OSGeo4W runtime DLLs will be installed from: ${QGIS_BIN_DIR}")
    else()
        message(STATUS "OSGeo4W bin dir not found at ${QGIS_BIN_DIR} — GDAL DLLs need manual PATH setup")
    endif()
endif()

install(DIRECTORY config/ DESTINATION etc/soilwatercm)
