# EnvConfig.cmake - 环境变量配置管理
#
# 此模块用于配置外部工具的环境变量
# 包括GRASS、SAGA、OTB、conda环境等
#
# 用法：
#   include(EnvConfig)
#   setup_external_tool_env()

cmake_minimum_required(VERSION 3.16)

# 设置外部工具环境变量
function(setup_external_tool_env)
    message(STATUS "")
    message(STATUS "=== Setting Up External Tool Environment ===")
    message(STATUS "")
    
    # GRASS GIS
    if(NOT DEFINED GRASS_HOME)
        if(WIN32)
            # 优先使用thirdparty/vendor目录
            if(EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/vendor/osgeo4w/apps/grass")
                set(GRASS_HOME "${CMAKE_SOURCE_DIR}/thirdparty/vendor/osgeo4w/apps/grass" CACHE PATH "GRASS GIS installation path")
            else()
                set(GRASS_HOME "$ENV{ProgramFiles}/GRASS GIS 8.3" CACHE PATH "GRASS GIS installation path")
            endif()
        elseif(APPLE)
            set(GRASS_HOME "/Applications/GRASS-8.3.app/Contents/MacOS" CACHE PATH "GRASS GIS installation path")
        else()
            set(GRASS_HOME "/usr/lib/grass83" CACHE PATH "GRASS GIS installation path")
        endif()
    endif()
    
    if(EXISTS "${GRASS_HOME}")
        message(STATUS "GRASS_HOME: ${GRASS_HOME}")
        set(ENV{GRASS_HOME} "${GRASS_HOME}")
    else()
        message(STATUS "GRASS_HOME not found (optional)")
    endif()
    
    # SAGA GIS
    if(NOT DEFINED SAGA_HOME)
        if(WIN32)
            # 优先使用thirdparty/vendor目录
            if(EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/vendor/osgeo4w/apps/saga")
                set(SAGA_HOME "${CMAKE_SOURCE_DIR}/thirdparty/vendor/osgeo4w/apps/saga" CACHE PATH "SAGA GIS installation path")
            else()
                set(SAGA_HOME "$ENV{ProgramFiles}/saga-8.0" CACHE PATH "SAGA GIS installation path")
            endif()
        else()
            set(SAGA_HOME "/usr/local" CACHE PATH "SAGA GIS installation path")
        endif()
    endif()
    
    if(EXISTS "${SAGA_HOME}")
        message(STATUS "SAGA_HOME: ${SAGA_HOME}")
        set(ENV{SAGA_HOME} "${SAGA_HOME}")
    else()
        message(STATUS "SAGA_HOME not found (optional)")
    endif()
    
    # OTB (Orfeo ToolBox)
    if(NOT DEFINED OTB_HOME)
        if(WIN32)
            # 优先使用thirdparty/vendor目录
            if(EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/vendor/otb")
                set(OTB_HOME "${CMAKE_SOURCE_DIR}/thirdparty/vendor/otb" CACHE PATH "OTB installation path")
            else()
                set(OTB_HOME "$ENV{ProgramFiles}/OTB-8.1" CACHE PATH "OTB installation path")
            endif()
        else()
            set(OTB_HOME "/usr/local" CACHE PATH "OTB installation path")
        endif()
    endif()
    
    if(EXISTS "${OTB_HOME}")
        message(STATUS "OTB_HOME: ${OTB_HOME}")
        set(ENV{OTB_HOME} "${OTB_HOME}")
    else()
        message(STATUS "OTB_HOME not found (optional)")
    endif()
    
    # SNAP (Sentinel Application Platform)
    if(NOT DEFINED SNAP_HOME)
        if(WIN32)
            # 优先使用thirdparty/vendor目录
            if(EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/vendor/snap")
                set(SNAP_HOME "${CMAKE_SOURCE_DIR}/thirdparty/vendor/snap" CACHE PATH "SNAP installation path")
            else()
                set(SNAP_HOME "$ENV{ProgramFiles}/snap" CACHE PATH "SNAP installation path")
            endif()
        elseif(APPLE)
            set(SNAP_HOME "/Applications/snap.app/Contents" CACHE PATH "SNAP installation path")
        else()
            set(SNAP_HOME "/opt/snap" CACHE PATH "SNAP installation path")
        endif()
    endif()
    
    if(EXISTS "${SNAP_HOME}")
        message(STATUS "SNAP_HOME: ${SNAP_HOME}")
        set(ENV{SNAP_HOME} "${SNAP_HOME}")
    else()
        message(STATUS "SNAP_HOME not found (optional)")
    endif()
    
    # Conda环境
    if(NOT DEFINED MINTPY_ENV)
        set(MINTPY_ENV "mintpy" CACHE STRING "MintPy conda environment name")
    endif()
    message(STATUS "MINTPY_ENV: ${MINTPY_ENV}")
    set(ENV{MINTPY_ENV} "${MINTPY_ENV}")
    
    if(NOT DEFINED ISCE2_ENV)
        set(ISCE2_ENV "isce2" CACHE STRING "ISCE2 conda environment name")
    endif()
    message(STATUS "ISCE2_ENV: ${ISCE2_ENV}")
    set(ENV{ISCE2_ENV} "${ISCE2_ENV}")
    
    # SNAPHU路径
    if(NOT DEFINED SNAPHU_PATH)
        find_program(SNAPHU_EXE snaphu)
        if(SNAPHU_EXE)
            get_filename_component(SNAPHU_PATH "${SNAPHU_EXE}" DIRECTORY)
            set(SNAPHU_PATH "${SNAPHU_PATH}" CACHE PATH "SNAPHU installation path")
        endif()
    endif()
    
    if(DEFINED SNAPHU_PATH AND EXISTS "${SNAPHU_PATH}")
        message(STATUS "SNAPHU_PATH: ${SNAPHU_PATH}")
        set(ENV{SNAPHU_PATH} "${SNAPHU_PATH}")
    else()
        message(STATUS "SNAPHU_PATH not found (will use static library)")
    endif()
    
    message(STATUS "")
endfunction()

# 验证环境变量
function(validate_env_vars)
    message(STATUS "")
    message(STATUS "=== Validating Environment Variables ===")
    message(STATUS "")
    
    set(ALL_VALID TRUE)
    
    # 检查GRASS
    if(DEFINED ENV{GRASS_HOME})
        if(EXISTS "$ENV{GRASS_HOME}")
            message(STATUS "✓ GRASS_HOME: $ENV{GRASS_HOME}")
        else()
            message(WARNING "✗ GRASS_HOME set but path does not exist: $ENV{GRASS_HOME}")
            set(ALL_VALID FALSE)
        endif()
    else()
        message(STATUS "○ GRASS_HOME not set (optional)")
    endif()
    
    # 检查SAGA
    if(DEFINED ENV{SAGA_HOME})
        if(EXISTS "$ENV{SAGA_HOME}")
            message(STATUS "✓ SAGA_HOME: $ENV{SAGA_HOME}")
        else()
            message(WARNING "✗ SAGA_HOME set but path does not exist: $ENV{SAGA_HOME}")
            set(ALL_VALID FALSE)
        endif()
    else()
        message(STATUS "○ SAGA_HOME not set (optional)")
    endif()
    
    # 检查OTB
    if(DEFINED ENV{OTB_HOME})
        if(EXISTS "$ENV{OTB_HOME}")
            message(STATUS "✓ OTB_HOME: $ENV{OTB_HOME}")
        else()
            message(WARNING "✗ OTB_HOME set but path does not exist: $ENV{OTB_HOME}")
            set(ALL_VALID FALSE)
        endif()
    else()
        message(STATUS "○ OTB_HOME not set (optional)")
    endif()
    
    # 检查SNAP
    if(DEFINED ENV{SNAP_HOME})
        if(EXISTS "$ENV{SNAP_HOME}")
            message(STATUS "✓ SNAP_HOME: $ENV{SNAP_HOME}")
        else()
            message(WARNING "✗ SNAP_HOME set but path does not exist: $ENV{SNAP_HOME}")
            set(ALL_VALID FALSE)
        endif()
    else()
        message(STATUS "○ SNAP_HOME not set (optional)")
    endif()
    
    # 检查conda环境
    find_program(CONDA_EXE conda)
    if(CONDA_EXE)
        # 检查MintPy环境
        if(DEFINED ENV{MINTPY_ENV})
            execute_process(
                COMMAND ${CONDA_EXE} env list
                OUTPUT_VARIABLE CONDA_ENV_LIST
                ERROR_QUIET
            )
            string(FIND "${CONDA_ENV_LIST}" "$ENV{MINTPY_ENV}" MINTPY_ENV_FOUND)
            if(MINTPY_ENV_FOUND GREATER -1)
                message(STATUS "✓ MINTPY_ENV: $ENV{MINTPY_ENV}")
            else()
                message(STATUS "○ MINTPY_ENV '$ENV{MINTPY_ENV}' not found in conda (optional, InSAR time-series disabled)")
            endif()
        endif()
        
        # 检查ISCE2环境
        if(DEFINED ENV{ISCE2_ENV})
            string(FIND "${CONDA_ENV_LIST}" "$ENV{ISCE2_ENV}" ISCE2_ENV_FOUND)
            if(ISCE2_ENV_FOUND GREATER -1)
                message(STATUS "✓ ISCE2_ENV: $ENV{ISCE2_ENV}")
            else()
                message(STATUS "○ ISCE2_ENV '$ENV{ISCE2_ENV}' not found in conda (optional, InSAR D-InSAR disabled on Linux)")
            endif()
        endif()
    else()
        message(STATUS "○ conda not found (InSAR tools will be disabled)")
    endif()
    
    message(STATUS "")
    
    if(ALL_VALID)
        message(STATUS "All configured environment variables are valid")
    else()
        message(WARNING "Some environment variables are invalid - check the warnings above")
    endif()
    
    set(ENV_VALID ${ALL_VALID} PARENT_SCOPE)
endfunction()

# 生成环境配置文件模板
function(generate_env_config_template OUTPUT_FILE)
    file(WRITE "${OUTPUT_FILE}" "# SoilWaterCM Environment Configuration Template
# Copy this file to .env and customize for your environment

# === GIS Tools ===

# GRASS GIS (optional, used via QGIS Processing)
# GRASS_HOME=/path/to/grass

# SAGA GIS (optional, used via QGIS Processing)
# SAGA_HOME=/path/to/saga

# Orfeo ToolBox (optional, for advanced image processing)
# OTB_HOME=/path/to/otb

# === InSAR Tools ===

# SNAP (Sentinel Application Platform)
# Required for InSAR processing on Windows
# SNAP_HOME=/path/to/snap

# ISCE2 conda environment name
# Required for InSAR processing on Linux/macOS
ISCE2_ENV=isce2

# MintPy conda environment name
# Required for time-series InSAR analysis
MINTPY_ENV=mintpy

# SNAPHU (phase unwrapping)
# Optional - will use static library if not set
# SNAPHU_PATH=/path/to/snaphu

# === QGIS ===

# QGIS installation path (optional)
# QGIS will be auto-detected if not set
# QGIS_PREFIX_PATH=/path/to/qgis

# === Database ===

# PostgreSQL connection
POSTGRES_HOST=localhost
POSTGRES_PORT=5432
POSTGRES_DB=soilwatercm
POSTGRES_USER=postgres
# POSTGRES_PASSWORD=your_password

# MongoDB connection
MONGODB_HOST=localhost
MONGODB_PORT=27017
MONGODB_DB=soilwatercm_meta

# Redis connection
REDIS_HOST=localhost
REDIS_PORT=6379

# === Storage ===

# Base directory for data storage
STORAGE_BASE_DIR=data
STORAGE_RAW_DIR=data/raw
STORAGE_PROCESSED_DIR=data/processed
STORAGE_RESULTS_DIR=data/results

# === Server ===

# Server configuration
SERVER_HOST=0.0.0.0
SERVER_PORT=8080
SERVER_WORKERS=4
")
    
    message(STATUS "Environment config template written to: ${OUTPUT_FILE}")
endfunction()

# 主函数：设置环境配置
function(setup_env_config)
    setup_external_tool_env()
    validate_env_vars()
    
    # 生成配置模板
    set(TEMPLATE_FILE "${CMAKE_SOURCE_DIR}/config/.env.template")
    generate_env_config_template("${TEMPLATE_FILE}")
    
    # 提示用户
    message(STATUS "")
    message(STATUS "=== Environment Configuration Complete ===")
    message(STATUS "")
    message(STATUS "A template has been generated at: ${TEMPLATE_FILE}")
    message(STATUS "Copy it to .env and customize for your environment:")
    message(STATUS "  cp ${TEMPLATE_FILE} ${CMAKE_SOURCE_DIR}/config/.env")
    message(STATUS "")
endfunction()
