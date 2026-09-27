cmake_minimum_required(VERSION 3.18.0 FATAL_ERROR)

project(kblayout LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Backend options (respects ebuild USE flags, defaults to auto-detect)
# Set to ON/OFF from ebuild, or leave undefined for auto-detect
if(NOT DEFINED KBLAYOUT_X11)
    set(KBLAYOUT_X11 AUTO)
endif()
if(NOT DEFINED KBLAYOUT_WAYLAND)
    set(KBLAYOUT_WAYLAND AUTO)
endif()

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Xml DBus)
qt_standard_project_setup()

# LXQt
find_package(lxqt REQUIRED)

# XKBCommon (required)
find_package(XKBCommon REQUIRED COMPONENTS XKBCommon)

# X11 backend (optional)
find_package(XCB COMPONENTS XCB XKB)
find_package(XKBCommon COMPONENTS X11)

# Wayland backend (optional)
find_package(Wayland OPTIONAL_COMPONENTS Client)
find_package(EGL)

# Set up source files
set(HEADERS
    src/kblayout.h
    src/kblayoutwidget.h
    src/kblayoutbackend.h
    src/kblayoutsettings.h
    src/kblayoutsettingsdialog.h
)

set(SOURCES
    kblayout-plugin.cpp
    src/kblayout.cpp
    src/kblayoutwidget.cpp
    src/kblayoutbackend.cpp
    src/kblayoutsettings.cpp
    src/kblayoutsettingsdialog.cpp
)

set(LIBRARIES
    Qt6::Widgets
    Qt6::Xml
    Qt6::DBus
    lxqt
    XKBCommon::XKBCommon
)

# X11 backend
if(KBLAYOUT_X11 STREQUAL "AUTO")
    if(XCB_FOUND AND XKBCommon_X11_FOUND)
        set(_enable_x11 TRUE)
    else()
        set(_enable_x11 FALSE)
    endif()
elseif(KBLAYOUT_X11)
    set(_enable_x11 TRUE)
else()
    set(_enable_x11 FALSE)
endif()

if(_enable_x11)
    message(STATUS "kblayout: X11 backend enabled")
    list(APPEND HEADERS src/x11backend.h)
    list(APPEND SOURCES src/x11backend.cpp)
    list(APPEND LIBRARIES ${XCB_LIBRARIES} XKBCommon::X11 KF6::WindowSystem)
    add_definitions(-DKBLAYOUT_X11)
else()
    message(STATUS "kblayout: X11 backend disabled")
endif()

# Wayland backend
if(KBLAYOUT_WAYLAND STREQUAL "AUTO")
    if(Wayland_Client_FOUND)
        set(_enable_wayland TRUE)
    else()
        set(_enable_wayland FALSE)
    endif()
elseif(KBLAYOUT_WAYLAND)
    set(_enable_wayland TRUE)
else()
    set(_enable_wayland FALSE)
endif()

if(_enable_wayland)
    message(STATUS "kblayout: Wayland backend enabled")
    list(APPEND HEADERS src/waylandbackend.h)
    list(APPEND SOURCES src/waylandbackend.cpp)
    list(APPEND LIBRARIES Wayland::Client)
    add_definitions(-DKBLAYOUT_WAYLAND)
else()
    message(STATUS "kblayout: Wayland backend disabled")
endif()

# Build plugin as shared library
add_library(${PROJECT_NAME} MODULE
    ${HEADERS}
    ${SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARIES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lxqt-panel"
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lxqt-panel"
)

# Install targets
install(TARGETS ${PROJECT_NAME}
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/lxqt-panel"
)

# Install desktop file
install(FILES resources/kblayout.desktop.in
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/lxqt/lxqt-panel"
)

# Install translations (if any)
if(EXISTS "${CMAKE_SOURCE_DIR}/translations/kblayout.ts")
    find_package(Qt6LinguistTools)
    if(Qt6LinguistTools_FOUND)
        qt_add_translation(QM_FILES "${CMAKE_SOURCE_DIR}/translations/kblayout.ts")
        install(FILES ${QM_FILES}
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/lxqt/lxqt-panel/translations"
        )
    endif()
endif()
