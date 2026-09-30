cmake_minimum_required(VERSION 3.18.0 FATAL_ERROR)

project(kblayout LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(LXQtTranslate)

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

set(UI_FILES
    src/kblayoutsettingsdialog.ui
)

# Generate headers from UI files
qt_wrap_ui(UI_HEADERS ${UI_FILES})

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
elseif(KBLAYOUT_X11 STREQUAL "ON")
    set(_enable_x11 TRUE)
else()
    set(_enable_x11 FALSE)
endif()

if(_enable_x11)
    message(STATUS "kblayout: X11 backend enabled")
    list(APPEND HEADERS src/x11backend.h src/kbdkeeper.h)
    list(APPEND SOURCES src/x11backend.cpp)
    # XKBCommon::X11 provides libxkbcommon-x11, but we also need libxcb-xkb
    # for xcb_xkb_latch_lock_state()
    find_package(XCB REQUIRED COMPONENTS XKB)
    list(APPEND SOURCES src/kbdkeeper.cpp)
    list(APPEND LIBRARIES ${XCB_XKB_LIBRARIES} XKBCommon::X11 KF6::WindowSystem)
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
elseif(KBLAYOUT_WAYLAND STREQUAL "ON")
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

# Ensure at least one backend is enabled
if(NOT KBLAYOUT_X11 AND NOT KBLAYOUT_WAYLAND)
    message(FATAL_ERROR "kblayout: At least one backend must be enabled (KBLAYOUT_X11 or KBLAYOUT_WAYLAND)")
endif()

# Build plugin as shared library
add_library(${PROJECT_NAME} MODULE
    ${HEADERS}
    ${SOURCES}
    ${UI_HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBRARIES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lxqt-panel"
    INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lxqt-panel"
)

# Install targets
install(TARGETS ${PROJECT_NAME}
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/lxqt-panel"
)

# Install desktop file (install as .desktop, not .desktop.in)
install(FILES resources/kblayout.desktop.in
    RENAME kblayout.desktop
    DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/lxqt/lxqt-panel"
)

# Translations
lxqt_translate_ts(QM_FILES
    UPDATE_TRANSLATIONS ${UPDATE_TRANSLATIONS}
    SOURCES
        ${HEADERS}
        ${SOURCES}
        ${UI_HEADERS}
    TEMPLATE
        kblayout
    INSTALL_DIR
        "${LXQT_TRANSLATIONS_DIR}/lxqt-panel/kblayout"
)

# Translation loader
lxqt_plugin_translation_loader(QM_LOADER kblayout "lxqt-panel")

# Add loader and QM files to sources
list(APPEND SOURCES ${QM_LOADER} ${QM_FILES})

