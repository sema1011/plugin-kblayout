cmake_minimum_required(VERSION 3.18.0 FATAL_ERROR)

project(test-kblayout LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use Qt6 directly
list(APPEND CMAKE_PREFIX_PATH "/usr/lib64/cmake/Qt6")
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

enable_testing()
qt_standard_project_setup()

# ============================================================================
# Test 1: KbLayoutWidget
# ============================================================================
qt_add_executable(test-kblayoutwidget
    test_kblayoutwidget.cpp
    ../src/kblayoutwidget.cpp
    ../src/kblayoutbackend.cpp
)

target_link_libraries(test-kblayoutwidget PRIVATE
    Qt6::Widgets
    Qt6::Test
)

target_include_directories(test-kblayoutwidget PRIVATE ../src)

add_test(NAME test-kblayoutwidget COMMAND test-kblayoutwidget)

# ============================================================================
# Test 2: KbLayoutConfig
# ============================================================================
qt_add_executable(test-kblayoutconfig
    test_kblayoutconfig.cpp
)

target_link_libraries(test-kblayoutconfig PRIVATE
    Qt6::Widgets
    Qt6::Test
)

target_include_directories(test-kblayoutconfig PRIVATE ../src)

add_test(NAME test-kblayoutconfig COMMAND test-kblayoutconfig)
