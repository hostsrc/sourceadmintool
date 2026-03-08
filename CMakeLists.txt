cmake_minimum_required(VERSION 3.16)
project(SourceAdminTool VERSION 1.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

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
)

set(FORMS mainwindow.ui)
set(RESOURCES icons.qrc)

qt_add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${FORMS}
    ${RESOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniupnpc
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libmaxminddb
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE sourceadmin.rc)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 IPHlpApi)
    target_link_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniupnpc/lib/win
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libmaxminddb/lib/win
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE miniupnpc libmaxminddb)
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

    target_link_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniupnpc/lib/mac
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libmaxminddb/lib/mac
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE miniupnpc maxminddb)
else()
    # Linux
    set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/package
    )
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN/lib"
    )
    target_link_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/miniupnpc/lib/linux
        ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/libmaxminddb/lib/linux
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE miniupnpc maxminddb)
endif()
