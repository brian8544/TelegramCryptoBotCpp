cmake_minimum_required(VERSION 3.15)

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE PATH "vcpkg toolchain file")

project(TelegramCryptoBot)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

add_executable(TelegramCryptoBot src/Main.cpp)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

if (NOT nlohmann_json_FOUND)
    message(FATAL_ERROR "nlohmann_json library not found.")
endif()

target_include_directories(TelegramCryptoBot PRIVATE ${CURL_INCLUDE_DIR})
target_link_libraries(TelegramCryptoBot PRIVATE ${CURL_LIBRARIES} nlohmann_json::nlohmann_json)

find_path(TGBOT_INCLUDE_DIR tgbot/tgbot.h)
find_library(TGBOT_LIBRARY NAMES tgbot)

if (TGBOT_INCLUDE_DIR AND TGBOT_LIBRARY)
    target_include_directories(TelegramCryptoBot PRIVATE ${TGBOT_INCLUDE_DIR})
    target_link_libraries(TelegramCryptoBot PRIVATE ${TGBOT_LIBRARY})
else()
    message(FATAL_ERROR "TgBot library not found.")
endif()

# Custom preprocessor definitions
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_custom_command(
    TARGET TelegramCryptoBot POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/src/Config.conf
            $<TARGET_FILE_DIR:TelegramCryptoBot>/Config.conf
)