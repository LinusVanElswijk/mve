enable_testing()

find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message("GTest NOT found! tests will NOT be built!")
    return()
endif()
include_directories(${GTEST_INCLUDE_DIR})

add_subdirectory(math)
add_subdirectory(util)
add_subdirectory(mve)
