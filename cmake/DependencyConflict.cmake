# DependencyConflict.cmake - 依赖冲突检测机制
#
# 此模块用于检测GDAL、PROJ、QGIS库的版本冲突
# 生成冲突报告和建议
#
# 用法：
#   include(DependencyConflict)
#   check_dependency_conflicts()

cmake_minimum_required(VERSION 3.16)

# 检查GDAL版本冲突
function(check_gdal_conflict)
    # 获取系统GDAL版本
    find_package(GDAL QUIET)
    if(GDAL_FOUND)
        set(SYSTEM_GDAL_VERSION "${GDAL_VERSION}")
        message(STATUS "System GDAL version: ${SYSTEM_GDAL_VERSION}")
    endif()
    
    # 获取QGIS GDAL版本
    if(QGIS_FOUND AND QGIS_GDAL_VERSION)
        set(QGIS_GDAL_VER "${QGIS_GDAL_VERSION}")
        message(STATUS "QGIS GDAL version: ${QGIS_GDAL_VER}")
    endif()
    
    # 检查冲突
    if(SYSTEM_GDAL_VERSION AND QGIS_GDAL_VER)
        if(NOT SYSTEM_GDAL_VERSION STREQUAL QGIS_GDAL_VER)
            message(WARNING "")
            message(WARNING "=== GDAL VERSION CONFLICT DETECTED ===")
            message(WARNING "System GDAL version: ${SYSTEM_GDAL_VERSION}")
            message(WARNING "QGIS GDAL version:   ${QGIS_GDAL_VER}")
            message(WARNING "")
            message(WARNING "This may cause runtime errors due to ABI incompatibility.")
            message(WARNING "")
            message(WARNING "RECOMMENDED SOLUTION:")
            message(WARNING "  1. Use QGIS-provided GDAL (recommended)")
            message(WARNING "     cmake -DUSE_QGIS_GDAL=ON ..")
            message(WARNING "  2. Or install matching GDAL version system-wide")
            message(WARNING "  3. Or rebuild QGIS with system GDAL")
            message(WARNING "")
            set(GDAL_CONFLICT_DETECTED TRUE PARENT_SCOPE)
        else()
            message(STATUS "GDAL versions are consistent: ${SYSTEM_GDAL_VERSION}")
            set(GDAL_CONFLICT_DETECTED FALSE PARENT_SCOPE)
        endif()
    else()
        set(GDAL_CONFLICT_DETECTED FALSE PARENT_SCOPE)
    endif()
endfunction()

# 检查PROJ版本冲突
function(check_proj_conflict)
    # 获取系统PROJ版本
    find_package(PROJ QUIET)
    if(PROJ_FOUND)
        set(SYSTEM_PROJ_VERSION "${PROJ_VERSION}")
        message(STATUS "System PROJ version: ${SYSTEM_PROJ_VERSION}")
    endif()
    
    # 获取QGIS PROJ版本
    if(QGIS_FOUND AND QGIS_PROJ_VERSION)
        set(QGIS_PROJ_VER "${QGIS_PROJ_VERSION}")
        message(STATUS "QGIS PROJ version: ${QGIS_PROJ_VER}")
    endif()
    
    # 检查冲突
    if(SYSTEM_PROJ_VERSION AND QGIS_PROJ_VER)
        if(NOT SYSTEM_PROJ_VERSION STREQUAL QGIS_PROJ_VER)
            message(WARNING "")
            message(WARNING "=== PROJ VERSION CONFLICT DETECTED ===")
            message(WARNING "System PROJ version: ${SYSTEM_PROJ_VERSION}")
            message(WARNING "QGIS PROJ version:   ${QGIS_PROJ_VER}")
            message(WARNING "")
            message(WARNING "This may cause coordinate transformation errors.")
            message(WARNING "")
            message(WARNING "RECOMMENDED SOLUTION:")
            message(WARNING "  1. Use QGIS-provided PROJ (recommended)")
            message(WARNING "     cmake -DUSE_QGIS_PROJ=ON ..")
            message(WARNING "  2. Or install matching PROJ version system-wide")
            message(WARNING "  3. Or update PROJ data files (proj.db)")
            message(WARNING "")
            set(PROJ_CONFLICT_DETECTED TRUE PARENT_SCOPE)
        else()
            message(STATUS "PROJ versions are consistent: ${SYSTEM_PROJ_VERSION}")
            set(PROJ_CONFLICT_DETECTED FALSE PARENT_SCOPE)
        endif()
    else()
        set(PROJ_CONFLICT_DETECTED FALSE PARENT_SCOPE)
    endif()
endfunction()

# 检查QGIS库与系统库冲突
function(check_qgis_system_conflict)
    if(NOT QGIS_FOUND)
        return()
    endif()
    
    # 检查QGIS库路径是否在系统库路径中
    get_filename_component(QGIS_LIB_DIR "${QGIS_GDAL_LIBRARY}" DIRECTORY)
    
    # 获取系统库路径
    string(REPLACE ":" ";" SYSTEM_LIB_PATHS "$ENV{LD_LIBRARY_PATH}")
    if(NOT SYSTEM_LIB_PATHS)
        set(SYSTEM_LIB_PATHS "/usr/lib" "/usr/local/lib")
    endif()
    
    set(QGIS_LIB_IN_SYSTEM_PATH FALSE)
    foreach(SYSTEM_LIB_PATH ${SYSTEM_LIB_PATHS})
        if(QGIS_LIB_DIR STREQUAL SYSTEM_LIB_PATH)
            set(QGIS_LIB_IN_SYSTEM_PATH TRUE)
            break()
        endif()
    endforeach()
    
    if(NOT QGIS_LIB_IN_SYSTEM_PATH)
        message(STATUS "QGIS libraries are in isolated path: ${QGIS_LIB_DIR}")
        message(STATUS "This is GOOD - avoids conflicts with system libraries")
    else()
        message(WARNING "")
        message(WARNING "=== QGIS LIBRARY PATH WARNING ===")
        message(WARNING "QGIS libraries are in system library path: ${QGIS_LIB_DIR}")
        message(WARNING "This may cause conflicts with other applications.")
        message(WARNING "")
        message(WARNING "RECOMMENDED:")
        message(WARNING "  Consider installing QGIS in isolated prefix")
        message(WARNING "  Or use RPATH to control library search path")
        message(WARNING "")
    endif()
endfunction()

# 生成冲突报告
function(generate_conflict_report REPORT_FILE)
    file(WRITE "${REPORT_FILE}" "# Dependency Conflict Report\n")
    file(APPEND "${REPORT_FILE}" "# Generated by CMake\n\n")
    
    file(APPEND "${REPORT_FILE}" "## GDAL\n")
    if(GDAL_FOUND)
        file(APPEND "${REPORT_FILE}" "- System GDAL: ${GDAL_VERSION}\n")
    endif()
    if(QGIS_GDAL_VERSION)
        file(APPEND "${REPORT_FILE}" "- QGIS GDAL: ${QGIS_GDAL_VERSION}\n")
    endif()
    if(GDAL_CONFLICT_DETECTED)
        file(APPEND "${REPORT_FILE}" "- **CONFLICT DETECTED**\n")
    else()
        file(APPEND "${REPORT_FILE}" "- No conflict\n")
    endif()
    
    file(APPEND "${REPORT_FILE}" "\n## PROJ\n")
    if(PROJ_FOUND)
        file(APPEND "${REPORT_FILE}" "- System PROJ: ${PROJ_VERSION}\n")
    endif()
    if(QGIS_PROJ_VERSION)
        file(APPEND "${REPORT_FILE}" "- QGIS PROJ: ${QGIS_PROJ_VERSION}\n")
    endif()
    if(PROJ_CONFLICT_DETECTED)
        file(APPEND "${REPORT_FILE}" "- **CONFLICT DETECTED**\n")
    else()
        file(APPEND "${REPORT_FILE}" "- No conflict\n")
    endif()
    
    file(APPEND "${REPORT_FILE}" "\n## QGIS\n")
    if(QGIS_FOUND)
        file(APPEND "${REPORT_FILE}" "- QGIS Version: ${QGIS_VERSION}\n")
        file(APPEND "${REPORT_FILE}" "- QGIS Prefix: ${QGIS_PREFIX_PATH}\n")
    else()
        file(APPEND "${REPORT_FILE}" "- QGIS not found\n")
    endif()
    
    message(STATUS "Conflict report written to: ${REPORT_FILE}")
endfunction()

# 主函数：检查所有依赖冲突
function(check_dependency_conflicts)
    message(STATUS "")
    message(STATUS "=== Checking Dependency Conflicts ===")
    message(STATUS "")
    
    check_gdal_conflict()
    check_proj_conflict()
    check_qgis_system_conflict()
    
    # 生成报告
    set(REPORT_FILE "${CMAKE_BINARY_DIR}/dependency_conflict_report.md")
    generate_conflict_report("${REPORT_FILE}")
    
    # 总结
    if(GDAL_CONFLICT_DETECTED OR PROJ_CONFLICT_DETECTED)
        message(WARNING "")
        message(WARNING "=== DEPENDENCY CONFLICTS DETECTED ===")
        message(WARNING "Please review the conflict report:")
        message(WARNING "  ${REPORT_FILE}")
        message(WARNING "")
        
        # 提供解决方案建议
        message(WARNING "QUICK FIX:")
        message(WARNING "  cmake -DUSE_QGIS_DEPS=ON ..")
        message(WARNING "")
        
        # 询问用户是否继续
        if(NOT IGNORE_CONFLICTS)
            message(FATAL_ERROR "Dependency conflicts detected. Use -DIGNORE_CONFLICTS=ON to continue anyway.")
        endif()
    else()
        message(STATUS "")
        message(STATUS "=== No Dependency Conflicts ===")
        message(STATUS "")
    endif()
endfunction()
