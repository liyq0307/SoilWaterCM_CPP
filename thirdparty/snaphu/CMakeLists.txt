cmake_minimum_required(VERSION 3.16)
project(snaphu C)

set(SNAPHU_VERSION "1.4.2")

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/snaphu.c")
    message(STATUS "SNAPHU source not found. Please download from:")
    message(STATUS "  https://web.stanford.edu/group/radar/softwareandlinks/sw/snaphu/snaphu_1.4.2.tar.gz")
    message(STATUS "Extract to: ${CMAKE_CURRENT_SOURCE_DIR}")
    message(STATUS "Required files: snaphu.c, snaphu.h, island.c, snaphu.conf")
    
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/README.txt"
"SNAPHU Source Code Required
============================

Please download SNAPHU source code from:
  https://web.stanford.edu/group/radar/softwareandlinks/sw/snaphu/snaphu_1.4.2.tar.gz

Extract the following files to this directory:
  - snaphu.c
  - snaphu.h
  - island.c
  - snaphu.conf (optional, for default configuration)

After extracting, re-run CMake configuration.
")
    
    set(SNAPHU_SOURCES_AVAILABLE OFF)
else()
    set(SNAPHU_SOURCES_AVAILABLE ON)
endif()

if(SNAPHU_SOURCES_AVAILABLE)
    set(SNAPHU_SOURCES
        snaphu.c
        island.c
    )
    
    add_library(snaphu STATIC ${SNAPHU_SOURCES})
    
    target_include_directories(snaphu
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:include>
    )
    
    if(MSVC)
        target_compile_options(snaphu PRIVATE
            /W3
            /D_CRT_SECURE_NO_WARNINGS
        )
    else()
        target_compile_options(snaphu PRIVATE
            -Wall
            -Wextra
            -O3
        )
    endif()
    
    set_target_properties(snaphu PROPERTIES
        VERSION ${SNAPHU_VERSION}
        SOVERSION 1
        POSITION_INDEPENDENT_CODE ON
    )
    
    install(TARGETS snaphu
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
    )
    
    install(FILES snaphu.h
        DESTINATION include
    )
    
    message(STATUS "SNAPHU static library configured successfully")
    message(STATUS "  Version: ${SNAPHU_VERSION}")
    message(STATUS "  Sources: ${SNAPHU_SOURCES}")
else()
    message(WARNING "SNAPHU static library will not be built - source code not available")
endif()

option(SNAPHU_BUILD_CLI "Build SNAPHU command-line tool" OFF)

if(SNAPHU_BUILD_CLI AND SNAPHU_SOURCES_AVAILABLE)
    add_executable(snaphu_cli snaphu.c island.c)
    target_link_libraries(snaphu_cli PRIVATE m)
    set_target_properties(snaphu_cli PROPERTIES OUTPUT_NAME snaphu)
    
    install(TARGETS snaphu_cli
        RUNTIME DESTINATION bin
    )
endif()
