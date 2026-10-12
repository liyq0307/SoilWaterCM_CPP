# FindQGIS.cmake - 查找QGIS安装路径及其依赖库
#
# 此模块用于查找QGIS安装路径，并探测其自带的GDAL、PROJ、GEOS库
# 优先使用QGIS自带的库版本，避免版本冲突
#
# 用法：
#   find_package(QGIS REQUIRED)
#
# 定义的变量：
#   QGIS_FOUND          - 是否找到QGIS
#   QGIS_VERSION        - QGIS版本号
#   QGIS_PREFIX_PATH    - QGIS安装路径
#   QGIS_GDAL_LIBRARY   - QGIS自带的GDAL库
#   QGIS_PROJ_LIBRARY   - QGIS自带的PROJ库
#   QGIS_GEOS_LIBRARY   - QGIS自带的GEOS库
#   QGIS_GDAL_INCLUDE_DIR - GDAL头文件路径
#   QGIS_PROJ_INCLUDE_DIR - PROJ头文件路径
#   QGIS_GEOS_INCLUDE_DIR - GEOS头文件路径

cmake_minimum_required(VERSION 3.16)

if(QGIS_FOUND)
    return()
endif()

# 定义QGIS搜索路径
if(WIN32)
    # 优先搜索项目thirdparty/vendor目录
    set(QGIS_SEARCH_PATHS
        "${CMAKE_SOURCE_DIR}/thirdparty/vendor/osgeo4w"
        "${CMAKE_SOURCE_DIR}/thirdparty/vendor"
    )
    
    # 扫描所有固定驱动器上的 Program Files 目录
    execute_process(
        COMMAND powershell -NoProfile -Command "
            Get-WmiObject Win32_LogicalDisk | Where-Object { \$_.DriveType -eq 3 } |
            ForEach-Object { \$_.DeviceID }
        "
        OUTPUT_VARIABLE QGIS_DRIVES
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(QGIS_DRIVES)
        string(REGEX REPLACE "\r?\n" ";" QGIS_DRIVES_LIST "${QGIS_DRIVES}")
        foreach(DRIVE ${QGIS_DRIVES_LIST})
            string(STRIP "${DRIVE}" DRIVE_CLEAN)
            string(REGEX REPLACE ":" "" DRIVE_LETTER "${DRIVE_CLEAN}")
            if(DRIVE_LETTER)
                foreach(QGIS_VER
                    "QGIS 3.40.11" "QGIS 3.40.10" "QGIS 3.40.9" "QGIS 3.40.8" "QGIS 3.40.7"
                    "QGIS 3.40.6"  "QGIS 3.40.5"  "QGIS 3.40.4" "QGIS 3.40.3" "QGIS 3.40.2"
                    "QGIS 3.40.1"  "QGIS 3.40"
                    "QGIS 3.34.15" "QGIS 3.34.14" "QGIS 3.34.13" "QGIS 3.34.12" "QGIS 3.34.11"
                    "QGIS 3.34.10" "QGIS 3.34.9"  "QGIS 3.34.8"  "QGIS 3.34.7"  "QGIS 3.34.6"
                    "QGIS 3.34.5"  "QGIS 3.34.4"  "QGIS 3.34.3"  "QGIS 3.34.2"  "QGIS 3.34.1"
                    "QGIS 3.34"
                    "QGIS 3.28" "QGIS 3.22")
                    list(APPEND QGIS_SEARCH_PATHS
                        "${DRIVE_LETTER}:/Program Files/${QGIS_VER}"
                    )
                endforeach()
            endif()
        endforeach()
    endif()
    list(APPEND QGIS_SEARCH_PATHS
        "C:/Program Files (x86)/QGIS 3.40"
        "C:/Program Files (x86)/QGIS 3.34"
        "C:/Program Files (x86)/QGIS 3.28"
        "C:/Program Files (x86)/QGIS 3.22"
        "C:/OSGeo4W"
        "C:/QGIS"
        "D:/OSGeo4W"
        "D:/QGIS"
    )
    # 也查询注册表中的 QGIS 安装路径
    execute_process(
        COMMAND powershell -NoProfile -Command "
            Get-ItemProperty HKLM:\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\*,
                HKLM:\\SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\* 2>`$null |
            Where-Object { \$_.DisplayName -like '*QGIS*' -and \$_.InstallLocation } |
            Select-Object -ExpandProperty InstallLocation
        "
        OUTPUT_VARIABLE QGIS_REG_PATH
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(QGIS_REG_PATH)
        string(STRIP "${QGIS_REG_PATH}" QGIS_REG_PATH)
        list(APPEND QGIS_SEARCH_PATHS "${QGIS_REG_PATH}")
    endif()
elseif(APPLE)
    set(QGIS_SEARCH_PATHS
        "/Applications/QGIS.app/Contents/MacOS"
        "/usr/local/qgis"
        "/opt/qgis"
    )
else()
    set(QGIS_SEARCH_PATHS
        "/usr/local"
        "/usr"
        "/opt/qgis"
        "/opt/qgis-ltr"
    )
endif()

# 查找QGIS可执行文件
find_program(QGIS_EXECUTABLE
    NAMES qgis qgis-bin qgis-ltr-bin qgis-ltr-dev-bin qgis-rel-bin qgis-dev-bin
    HINTS ${QGIS_SEARCH_PATHS}
    PATH_SUFFIXES bin apps/qgis/bin apps/qgis-ltr/bin apps/qgis-ltr-dev/bin
)

if(QGIS_EXECUTABLE)
    # 从可执行文件路径推断安装路径
    get_filename_component(QGIS_BIN_DIR "${QGIS_EXECUTABLE}" DIRECTORY)
    get_filename_component(QGIS_PREFIX_PATH "${QGIS_BIN_DIR}" DIRECTORY)
    
    message(STATUS "Found QGIS at: ${QGIS_PREFIX_PATH}")
    
    # 查找QGIS版本
    if(EXISTS "${QGIS_PREFIX_PATH}/qgis-metadata.ini")
        file(STRINGS "${QGIS_PREFIX_PATH}/qgis-metadata.ini" QGIS_VERSION_LINE REGEX "^version=")
        if(QGIS_VERSION_LINE)
            string(REGEX REPLACE "^version=([0-9]+\\.[0-9]+\\.[0-9]+).*$" "\\1" QGIS_VERSION "${QGIS_VERSION_LINE}")
        endif()
    elseif(WIN32)
        # Windows: 从注册表或文件名推断版本
        string(REGEX MATCH "QGIS ([0-9]+\\.[0-9]+)" _ "${QGIS_PREFIX_PATH}")
        if(CMAKE_MATCH_1)
            set(QGIS_VERSION "${CMAKE_MATCH_1}")
        else()
            set(QGIS_VERSION "3.34")
        endif()
    else()
        # Linux/macOS: 使用qgis_process获取版本
        find_program(QGIS_PROCESS qgis_process HINTS "${QGIS_BIN_DIR}")
        if(QGIS_PROCESS)
            execute_process(
                COMMAND ${QGIS_PROCESS} --version
                OUTPUT_VARIABLE QGIS_VERSION_OUTPUT
                ERROR_QUIET
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" QGIS_VERSION "${QGIS_VERSION_OUTPUT}")
        endif()
    endif()
    
    if(NOT QGIS_VERSION)
        set(QGIS_VERSION "3.34")
    endif()
    
    message(STATUS "QGIS version: ${QGIS_VERSION}")
    
    # 查找QGIS自带的GDAL库
    if(WIN32)
        find_library(QGIS_GDAL_LIBRARY
            NAMES gdal gdal_i gdal313 gdal311 gdal308 gdal307
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES
                lib bin
                apps/qgis/lib
                apps/qgis-ltr/lib
                apps/qgis-ltr-dev/lib
                apps/gdal/lib
        )
        find_path(QGIS_GDAL_INCLUDE_DIR
            NAMES gdal.h
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES
                include
                apps/qgis/include
                apps/qgis-ltr/include
                apps/qgis-ltr-dev/include
                apps/gdal/include
                include/gdal
        )
    elseif(APPLE)
        find_library(QGIS_GDAL_LIBRARY
            NAMES gdal
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES lib lib/qgis
        )
        find_path(QGIS_GDAL_INCLUDE_DIR
            NAMES gdal.h
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES include include/gdal
        )
    else()
        find_library(QGIS_GDAL_LIBRARY
            NAMES gdal
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES lib lib/x86_64-linux-gnu
        )
        find_path(QGIS_GDAL_INCLUDE_DIR
            NAMES gdal.h
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES include include/gdal
        )
    endif()
    
    # 查找QGIS自带的PROJ库
    if(WIN32)
        find_library(QGIS_PROJ_LIBRARY
            NAMES proj proj_8 proj_9
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES
                lib bin
                apps/qgis/lib
                apps/qgis-ltr/lib
        )
        find_path(QGIS_PROJ_INCLUDE_DIR
            NAMES proj.h
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES
                include
                apps/qgis/include
                apps/qgis-ltr/include
                include/proj
        )
    else()
        find_library(QGIS_PROJ_LIBRARY
            NAMES proj
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES lib lib/x86_64-linux-gnu
        )
        find_path(QGIS_PROJ_INCLUDE_DIR
            NAMES proj.h
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES include include/proj
        )
    endif()
    
    # 查找QGIS自带的GEOS库
    if(WIN32)
        find_library(QGIS_GEOS_LIBRARY
            NAMES geos geos_c
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES
                lib bin
                apps/qgis/lib
                apps/qgis-ltr/lib
        )
        find_path(QGIS_GEOS_INCLUDE_DIR
            NAMES geos.h
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES
                include
                apps/qgis/include
                apps/qgis-ltr/include
                include/geos
        )
    else()
        find_library(QGIS_GEOS_LIBRARY
            NAMES geos geos_c
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES lib lib/x86_64-linux-gnu
        )
        find_path(QGIS_GEOS_INCLUDE_DIR
            NAMES geos.h
            HINTS "${QGIS_PREFIX_PATH}"
            PATH_SUFFIXES include include/geos
        )
    endif()
    
    # 版本兼容性检查
    if(QGIS_GDAL_LIBRARY)
        # 尝试从库文件名推断GDAL版本
        get_filename_component(GDAL_LIB_NAME "${QGIS_GDAL_LIBRARY}" NAME)
        string(REGEX MATCH "gdal([0-9]+)" GDAL_VERSION_NUM "${GDAL_LIB_NAME}")
        if(CMAKE_MATCH_1)
            set(QGIS_GDAL_VERSION "${CMAKE_MATCH_1}")
            if(CMAKE_MATCH_1 LESS 308)
                message(WARNING "GDAL version ${CMAKE_MATCH_1} is too old, minimum required is 3.8")
                set(QGIS_GDAL_COMPATIBLE FALSE)
            else()
                set(QGIS_GDAL_COMPATIBLE TRUE)
            endif()
        else()
            set(QGIS_GDAL_COMPATIBLE TRUE)
            set(QGIS_GDAL_VERSION "unknown")
        endif()
        message(STATUS "QGIS GDAL library: ${QGIS_GDAL_LIBRARY} (version: ${QGIS_GDAL_VERSION})")
    endif()
    
    if(QGIS_PROJ_LIBRARY)
        # 尝试从库文件名推断PROJ版本
        get_filename_component(PROJ_LIB_NAME "${QGIS_PROJ_LIBRARY}" NAME)
        string(REGEX MATCH "proj_([0-9]+)" PROJ_VERSION_NUM "${PROJ_LIB_NAME}")
        if(CMAKE_MATCH_1)
            set(QGIS_PROJ_VERSION "${CMAKE_MATCH_1}")
            if(CMAKE_MATCH_1 LESS 9)
                message(WARNING "PROJ version ${CMAKE_MATCH_1} is too old, minimum required is 9.0")
                set(QGIS_PROJ_COMPATIBLE FALSE)
            else()
                set(QGIS_PROJ_COMPATIBLE TRUE)
            endif()
        else()
            set(QGIS_PROJ_COMPATIBLE TRUE)
            set(QGIS_PROJ_VERSION "unknown")
        endif()
        message(STATUS "QGIS PROJ library: ${QGIS_PROJ_LIBRARY} (version: ${QGIS_PROJ_VERSION})")
    endif()
    
    if(QGIS_GEOS_LIBRARY)
        message(STATUS "QGIS GEOS library: ${QGIS_GEOS_LIBRARY}")
    endif()
    
    # 设置找到标志
    if(QGIS_GDAL_LIBRARY AND QGIS_PROJ_LIBRARY AND QGIS_GEOS_LIBRARY)
        set(QGIS_FOUND TRUE)
        message(STATUS "QGIS and its dependencies found successfully")
    else()
        set(QGIS_FOUND FALSE)
        message(WARNING "QGIS found but some dependencies are missing:")
        if(NOT QGIS_GDAL_LIBRARY)
            message(WARNING "  - GDAL library not found")
        endif()
        if(NOT QGIS_PROJ_LIBRARY)
            message(WARNING "  - PROJ library not found")
        endif()
        if(NOT QGIS_GEOS_LIBRARY)
            message(WARNING "  - GEOS library not found")
        endif()
    endif()
else()
    message(STATUS "QGIS not found")
    set(QGIS_FOUND FALSE)
endif()

# 提供使用QGIS库的辅助函数
function(use_qgis_dependencies TARGET)
    if(QGIS_FOUND)
        if(QGIS_GDAL_LIBRARY AND QGIS_GDAL_INCLUDE_DIR)
            target_include_directories(${TARGET} PRIVATE ${QGIS_GDAL_INCLUDE_DIR})
            target_link_libraries(${TARGET} PRIVATE ${QGIS_GDAL_LIBRARY})
        elseif(QGIS_GDAL_LIBRARY)
            # DLL-only (OSGeo4W): link via import lib from separate GDAL dev package
            target_link_libraries(${TARGET} PRIVATE ${QGIS_GDAL_LIBRARY})
            message(STATUS "QGIS GDAL DLL found (no headers in OSGeo4W install)")
        endif()
        
        if(QGIS_PROJ_LIBRARY AND QGIS_PROJ_INCLUDE_DIR)
            target_include_directories(${TARGET} PRIVATE ${QGIS_PROJ_INCLUDE_DIR})
            target_link_libraries(${TARGET} PRIVATE ${QGIS_PROJ_LIBRARY})
        endif()
        
        if(QGIS_GEOS_LIBRARY AND QGIS_GEOS_INCLUDE_DIR)
            target_include_directories(${TARGET} PRIVATE ${QGIS_GEOS_INCLUDE_DIR})
            target_link_libraries(${TARGET} PRIVATE ${QGIS_GEOS_LIBRARY})
        endif()
        
        # Export QGIS bin dir for runtime DLL resolution
        if(QGIS_BIN_DIR)
            set_property(TARGET ${TARGET} APPEND PROPERTY
                VS_DEBUGGER_ENVIRONMENT "PATH=${QGIS_BIN_DIR};$ENV{PATH}")
        endif()
        
        message(STATUS "Using QGIS dependencies for target: ${TARGET}")
    else()
        message(WARNING "QGIS not found, cannot use QGIS dependencies for target: ${TARGET}")
    endif()
endfunction()

# 标记高级变量
mark_as_advanced(
    QGIS_EXECUTABLE
    QGIS_PREFIX_PATH
    QGIS_GDAL_LIBRARY
    QGIS_GDAL_INCLUDE_DIR
    QGIS_PROJ_LIBRARY
    QGIS_PROJ_INCLUDE_DIR
    QGIS_GEOS_LIBRARY
    QGIS_GEOS_INCLUDE_DIR
)
