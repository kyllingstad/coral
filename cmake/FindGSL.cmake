cmake_minimum_required(VERSION 3.0)

find_path(GSL_INCLUDE_DIR "gsl/gsl" PATH_SUFFIXES "include")
if (GSL_INCLUDE_DIR)
    add_library(gsl INTERFACE IMPORTED)
    set_target_properties(gsl PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${GSL_INCLUDE_DIR}")

    set(GSL_LIBRARIES "gsl")
    set(GSL_INCLUDE_DIRS "${GSL_INCLUDE_DIR}")
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GSL DEFAULT_MSG GSL_LIBRARIES GSL_INCLUDE_DIRS)
