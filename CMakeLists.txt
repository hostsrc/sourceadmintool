cmake_minimum_required(VERSION 3.16)
project(SourceAdminTool VERSION 1.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Get git commit hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH_FULL
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()
if(NOT GIT_COMMIT_HASH_FULL)
    set(GIT_COMMIT_HASH_FULL "unknown")
endif()

# Generate version header from project version
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Widgets)

set(SOURCES
    main.cpp
    mainwindow.cpp
    query.cpp
    serverinfo.cpp
    settings.cpp
    worker.cpp
    customitems.cpp
    rcon.cpp
    simplecrypt.cpp
    main_events.cpp
    main_slots.cpp
    rcon_slots.cpp
    query_slots.cpp
    loghandler.cpp
    loghandler_slots.cpp
    util.cpp
    updatechecker.cpp
    serverbrowser.cpp
)

set(HEADERS
    mainwindow.h
    query.h
    serverinfo.h
    settings.h
    worker.h
    customitems.h
    rcon.h
    simplecrypt.h
    loghandler.h
    util.h
    updatechecker.h
    serverbrowser.h
)

set(FORMS mainwindow.ui)
set(RESOURCES icons.qrc)

qt_add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
    ${RESOURCES}
)

# Find third-party libraries (system or bundled)
find_library(MINIUPNPC_LIB NAMES miniupnpc)
find_path(MINIUPNPC_INCLUDE miniupnpc/miniupnpc.h)
find_library(MAXMINDDB_LIB NAMES maxminddb libmaxminddb)
find_path(MAXMINDDB_INCLUDE maxminddb.h)

if(MINIUPNPC_LIB AND MINIUPNPC_INCLUDE)
    message(STATUS "Found system miniupnpc: ${MINIUPNPC_LIB}")
    # System headers are under miniupnpc/ subfolder, code uses #include <miniupnpc.h>
    set(UPNPC_INCLUDE_DIR ${MINIUPNPC_INCLUDE}/miniupnpc)
    set(UPNPC_LIBRARY ${MINIUPNPC_LIB})
else()
    message(STATUS "Using bundled miniupnpc")
    set(UPNPC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniupnpc)
    if(WIN32)
        set(UPNPC_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniupnpc/lib/win/miniupnpc.lib)
    elseif(APPLE)
        set(UPNPC_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniupnpc/lib/mac/libminiupnpc.a)
    else()
        set(UPNPC_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniupnpc/lib/linux/libminiupnpc.a)
    endif()
endif()

if(MAXMINDDB_LIB AND MAXMINDDB_INCLUDE)
    message(STATUS "Found system libmaxminddb: ${MAXMINDDB_LIB}")
    set(MMDB_INCLUDE_DIR ${MAXMINDDB_INCLUDE})
    set(MMDB_LIBRARY ${MAXMINDDB_LIB})
else()
    message(STATUS "Using bundled libmaxminddb")
    set(MMDB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libmaxminddb)
    if(WIN32)
        set(MMDB_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libmaxminddb/lib/win/libmaxminddb.lib)
    elseif(APPLE)
        set(MMDB_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libmaxminddb/lib/mac/libmaxminddb.a)
    else()
        set(MMDB_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libmaxminddb/lib/linux/libmaxminddb.a)
    endif()
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${UPNPC_INCLUDE_DIR}
    ${MMDB_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    ${UPNPC_LIBRARY}
    ${MMDB_LIBRARY}
)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE sourceadmin.rc)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 IPHlpApi)
elseif(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_ICON_FILE icon.icns
    )
    set(APP_ICON_MACOSX ${CMAKE_CURRENT_SOURCE_DIR}/icons/icon.icns)
    set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_MACOSX})

    # Bundle data files
    set(BUNDLE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/GeoLite2-Country.mmdb
        ${CMAKE_CURRENT_SOURCE_DIR}/commands.xml
        ${CMAKE_CURRENT_SOURCE_DIR}/app_list_map.ini
    )
    foreach(FILE ${BUNDLE_FILES})
        set_source_files_properties(${FILE} PROPERTIES
            MACOSX_PACKAGE_LOCATION "MacOS"
        )
        target_sources(${PROJECT_NAME} PRIVATE ${FILE})
    endforeach()
else()
    # Linux
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN/lib"
    )
endif()
