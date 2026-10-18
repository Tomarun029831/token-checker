cmake_minimum_required(VERSION 3.20)

project(token-checker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ZXing CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(token-checker
    main.cpp
    camera_handler.cpp
	status_display.cpp
	notifier.cpp
)

target_include_directories(token-checker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(token-checker PRIVATE
	httplib::httplib
	OpenSSL::SSL
	OpenSSL::Crypto
    ZXing::Core
    ZXing::ZXing
	mf
    mfplat
    mfreadwrite
    mfuuid
    ole32
	propsys
)

target_compile_definitions(token-checker PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(token-checker PRIVATE /utf-8)
endif()
