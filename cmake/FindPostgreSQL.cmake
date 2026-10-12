find_path(PostgreSQL_INCLUDE_DIRS libpq-fe.h
    HINTS
        $ENV{PostgreSQL_ROOT}/include
        $ENV{PGHOME}/include
        /usr/include/postgresql
        /usr/local/include/postgresql
        "C:/Program Files/PostgreSQL/14/include"
        "C:/Program Files/PostgreSQL/15/include"
        "C:/Program Files/PostgreSQL/16/include"
)

find_library(PostgreSQL_LIBRARIES
    NAMES pq libpq
    HINTS
        $ENV{PostgreSQL_ROOT}/lib
        $ENV{PGHOME}/lib
        /usr/lib
        /usr/local/lib
        "C:/Program Files/PostgreSQL/14/lib"
        "C:/Program Files/PostgreSQL/15/lib"
        "C:/Program Files/PostgreSQL/16/lib"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PostgreSQL DEFAULT_MSG PostgreSQL_LIBRARIES PostgreSQL_INCLUDE_DIRS)

mark_as_advanced(PostgreSQL_INCLUDE_DIRS PostgreSQL_LIBRARIES)

if(PostgreSQL_FOUND AND NOT TARGET PostgreSQL::PostgreSQL)
    add_library(PostgreSQL::PostgreSQL UNKNOWN IMPORTED)
    set_target_properties(PostgreSQL::PostgreSQL PROPERTIES
        IMPORTED_LOCATION "${PostgreSQL_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${PostgreSQL_INCLUDE_DIRS}"
    )
endif()
