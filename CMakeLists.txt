cmake_minimum_required(VERSION 3.20)

project(token-checker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(apriltag CONFIG REQUIRED)
find_path(SIMDE_INCLUDE_DIRS "simde/arm/neon.h")

add_executable(token-checker
    main.cpp
    camera_handler.cpp
	status_display.cpp
	notifier.cpp
	requester.cpp
	token.cpp
	utility.cpp
)

target_include_directories(token-checker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
	${SIMDE_INCLUDE_DIRS}
)

target_link_libraries(token-checker PRIVATE
	apriltag::apriltag
	mf
    mfplat
    mfreadwrite
    mfuuid
    ole32
	propsys
	winhttp
)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

target_compile_definitions(token-checker PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    # /O1 : サイズ最小化セット (/Os, /Gy を含む)
    # /Gw : グローバルデータのセクション切り出し
    target_compile_options(token-checker PRIVATE /utf-8 /O1 /Gw)

    # /OPT:REF : 未使用の関数・データの削除
    # /OPT:ICF : 重複コードの統合
    # /INCREMENTAL:NO : 余分なパディング領域の除去
    target_link_options(token-checker PRIVATE /OPT:REF /OPT:ICF /INCREMENTAL:NO)
else()
    # -Os : サイズ優先最適化
    target_compile_options(token-checker PRIVATE -Os -ffunction-sections -fdata-sections)
    target_link_options(token-checker PRIVATE -Wl,--gc-sections -Wl,--strip-all)
endif()
