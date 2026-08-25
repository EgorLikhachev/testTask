cmake_minimum_required(VERSION 3.20)
project(mav-voice-gcs VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Test)

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
    src/announce/Announcer.h
    src/announce/Announcer.cpp
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

enable_testing()

add_executable(tst_parser tests/tst_parser.cpp)
target_link_libraries(tst_parser PRIVATE gcs_core Qt6::Test)
add_test(NAME parser COMMAND tst_parser)

add_executable(tst_domain tests/tst_domain.cpp)
target_link_libraries(tst_domain PRIVATE gcs_core Qt6::Test)
add_test(NAME domain COMMAND tst_domain)
