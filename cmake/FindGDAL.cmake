find_path(GDAL_INCLUDE_DIRS gdal.h
    HINTS
        $ENV{GDAL_ROOT}/include
        $ENV{GDAL_ROOT}/include/gdal
        /usr/include/gdal
        /usr/local/include/gdal
        "C:/Program Files/GDAL/include"
        "C:/OSGeo4W/include"
        "C:/QGIS 3.34/apps/qgis/include"
)

find_library(GDAL_LIBRARIES
    NAMES gdal gdal_i gdal201
    HINTS
        $ENV{GDAL_ROOT}/lib
        /usr/lib
        /usr/local/lib
        "C:/Program Files/GDAL/lib"
        "C:/OSGeo4W/lib"
        "C:/QGIS 3.34/apps/qgis/lib"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GDAL DEFAULT_MSG GDAL_LIBRARIES GDAL_INCLUDE_DIRS)

mark_as_advanced(GDAL_INCLUDE_DIRS GDAL_LIBRARIES)

if(GDAL_FOUND AND NOT TARGET GDAL::GDAL)
    add_library(GDAL::GDAL UNKNOWN IMPORTED)
    set_target_properties(GDAL::GDAL PROPERTIES
        IMPORTED_LOCATION "${GDAL_LIBRARIES}"
        INTERFACE_INCLUDE_DIRECTORIES "${GDAL_INCLUDE_DIRS}"
    )
endif()
