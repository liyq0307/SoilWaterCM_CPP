if(MSVC)
    # /MP 并行编译; 每个编译单元按核数全开。本项目 Drogon + C++20 的 TU 较重,
    # 全量重编时若内存不足 (C1060) 可设环境变量 CL_MPCount 限制并行数, 例如 CL_MPCount=4。
    add_compile_options(
        /W4
        /WX
        /permissive-
        /Zc:__cplusplus
        /Zc:preprocessor
        /MP
        /std:c++20
        /wd4100
    )
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++20")
    
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
    
else()
    add_compile_options(
        -Wall
        -Wextra
        -Werror
        -Wpedantic
        -Wconversion
        -Wold-style-cast
    )
    
    add_compile_options($<$<CONFIG:Release>:-O3 -march=native>)
    
    add_compile_options($<$<CONFIG:Debug>:-g -O0>)
    
    option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
    if(ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=modernize-*,cppcoreguidelines-*,bugprone-*")
endif()


