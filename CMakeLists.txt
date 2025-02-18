cmake_minimum_required(VERSION 3.14)
project(deribit_trading_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/libs/rest_api
    ${CMAKE_SOURCE_DIR}/libs/websocket
    ${CMAKE_SOURCE_DIR}/libs/order_placement
)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)

# Add env_handler library
add_library(env_handler
    libs/env_handler/env_handler.cpp
    libs/env_handler/env_handler.h
)

# WebSocket Client Library
add_library(websocket_client
    libs/websocket/websocket_client.cpp
    libs/websocket/websocket_client.h
)
target_link_libraries(websocket_client 
    PRIVATE
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

# WebSocket Server Library
add_library(websocket_server
    libs/websocket/websocket_server.cpp
    libs/websocket/websocket_server.h
)
target_link_libraries(websocket_server 
    PRIVATE
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)

# REST API Client Library
add_library(rest_client
    libs/rest_api/rest_client.cpp
    libs/rest_api/rest_client.h
)
target_link_libraries(rest_client 
    PRIVATE
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Order Placement Library
add_library(order_placement
    libs/order_placement/order_placement.cpp
    libs/order_placement/order_placement.h
)
target_link_libraries(order_placement 
    PRIVATE
    rest_client
    env_handler
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

add_library(websocket_manager
    libs/websocket/websocket_manager.cpp
    libs/websocket/websocket_manager.h
)

target_link_libraries(websocket_manager
    PRIVATE
    websocket_client
    websocket_server
    order_placement
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    pthread
)
# Main executable
add_executable(main src/main.cpp)
target_link_libraries(main 
    PRIVATE
    order_placement
    websocket_client
    websocket_server
    websocket_manager
    rest_client
    env_handler
    Boost::system
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
    CURL::libcurl
    nlohmann_json::nlohmann_json
    pthread
)

# Add include directories for each target
foreach(target 
    websocket_client 
    websocket_server 
    rest_client 
    order_placement 
    env_handler
)
    target_include_directories(${target}
        PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/libs/rest_api
        ${CMAKE_SOURCE_DIR}/libs/websocket
        ${CMAKE_SOURCE_DIR}/libs/order_placement
        ${CMAKE_SOURCE_DIR}/libs/env_handler
    )
endforeach()

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)



# Installation rules
install(TARGETS main
    RUNTIME DESTINATION bin
)

install(FILES config/settings.json 
    DESTINATION etc
)

# Optional: Set CURL paths explicitly if needed
#set(CURL_LIBRARY "/usr/lib/x86_64-linux-gnu/libcurl.so")
#set(CURL_INCLUDE_DIR "/usr/include/curl")