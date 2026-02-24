cmake_minimum_required(VERSION 3.20)
project(Wirewolf LANGUAGES CXX)

# Qt setup
find_package(Qt6 COMPONENTS Core Gui Widgets Sql REQUIRED)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Gecko SDK path must be supplied by the user
if(NOT DEFINED GECKO_SDK_PATH)
    message(FATAL_ERROR "Please provide -DGECKO_SDK_PATH=path/to/gecko/sdk")
endif()

# WebView2 setup

# The WebView2 SDK provides a header-only loader (`WebView2Loader.dll` at runtime).
# You must download the WebView2 SDK from Microsoft and unpack it somewhere,
# then set WEBVIEW2_SDK_PATH to that folder.  (See README for details.)
if(NOT DEFINED WEBVIEW2_SDK_PATH)
    message(FATAL_ERROR "Please provide -DWEBVIEW2_SDK_PATH=path/to/webview2/sdk")
endif()

include_directories(${WEBVIEW2_SDK_PATH}/include)
link_directories(${WEBVIEW2_SDK_PATH}/lib)

add_executable(Wirewolf
    src/main.cpp
)

# include Qt resource files
qt6_add_resources(RESOURCES src/resources.qrc)
target_sources(MyBrowser PRIVATE ${RESOURCES})

# link against Qt and WebView2 loader
# WebView2Loader.lib is provided by the SDK; the DLL is redistributed next to exe.
target_link_libraries(Wirewolf PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::Core
    # WebView2 loader library
    WebView2Loader
)

# set compiler flags
if(MSVC)
    target_compile_options(MyBrowser PRIVATE /W4 /EHsc)
else()
    target_compile_options(MyBrowser PRIVATE -Wall -Wextra -std=c++17)
endif()

# copy WebView2 runtime DLL next to executable for runtime (use tracker file from SDK)
add_custom_command(TARGET MyBrowser POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WEBVIEW2_SDK_PATH}/bin/WebView2Loader.dll" $<TARGET_FILE_DIR:MyBrowser>
)
