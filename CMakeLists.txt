cmake_minimum_required(VERSION 3.20)
project(mav-voice-gcs VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Test LinguistTools)

set(MAVLINK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/c_library_v2"
    CACHE PATH "Path to mavlink c_library_v2 checkout")

add_library(gcs_core STATIC
    src/config/AppConfig.h
    src/config/AppConfig.cpp
    src/transport/UdpTransport.h
    src/transport/UdpTransport.cpp
    src/mavlink/MavlinkParser.h
    src/mavlink/MavlinkParser.cpp
    src/mavlink/MavlinkCommands.h
    src/mavlink/MavlinkCommands.cpp
    src/mavlink/CopterModes.h
    src/mavlink/CopterModes.cpp
    src/domain/Telemetry.h
    src/domain/VehicleState.h
    src/domain/VehicleState.cpp
    src/domain/EventDetector.h
    src/domain/EventDetector.cpp
    src/domain/AntiSpamFilter.h
    src/domain/AntiSpamFilter.cpp
    src/domain/LinkMonitor.h
    src/domain/LinkMonitor.cpp
    src/announce/Announcer.h
    src/announce/Announcer.cpp
    src/telemetry/TlogWriter.h
    src/telemetry/TlogWriter.cpp
    src/tts/ITtsBackend.h
    src/tts/EspeakBackend.h
    src/tts/EspeakBackend.cpp
    src/tts/TtsQueue.h
    src/tts/TtsQueue.cpp
    src/app/Application.h
    src/app/Application.cpp
)

target_include_directories(gcs_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${MAVLINK_INCLUDE_DIR}
)

target_link_libraries(gcs_core PUBLIC Qt6::Core Qt6::Network)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(gcs_core PRIVATE -Wall -Wextra)
endif()

add_executable(gcs-tts
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
)
target_link_libraries(gcs-tts PRIVATE gcs_core Qt6::Widgets)

# Локализация: lrelease по .ts-файлам; .qm кладутся рядом с бинарником
# в <build>/translations и подхватываются main.cpp по локали.
file(GLOB TS_FILES "${CMAKE_CURRENT_SOURCE_DIR}/translations/*.ts")
if(TS_FILES)
    qt6_add_translation(QM_FILES ${TS_FILES})
    # Отдельная цель: custom-command lrelease сам по себе не строится,
    # пока на него никто не ссылается.
    add_custom_target(translations_all ALL DEPENDS ${QM_FILES})
    add_dependencies(gcs-tts translations_all)
    add_custom_command(TARGET gcs-tts POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                $<TARGET_FILE_DIR:gcs-tts>/translations
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${QM_FILES}
                $<TARGET_FILE_DIR:gcs-tts>/translations
        COMMENT "Копирование переводов (.qm)")
endif()

enable_testing()

add_executable(tst_parser tests/tst_parser.cpp)
target_link_libraries(tst_parser PRIVATE gcs_core Qt6::Test)
add_test(NAME parser COMMAND tst_parser)

add_executable(tst_domain tests/tst_domain.cpp)
target_link_libraries(tst_domain PRIVATE gcs_core Qt6::Test)
add_test(NAME domain COMMAND tst_domain)

add_executable(tst_tts tests/tst_tts.cpp)
target_link_libraries(tst_tts PRIVATE gcs_core Qt6::Test)
add_test(NAME tts COMMAND tst_tts)
