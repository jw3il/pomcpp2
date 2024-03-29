cmake_minimum_required(VERSION 3.0)

project(pomcpp VERSION 2.0)
set (CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)

OPTION(TEST_SHOW_GAME_PROGRESS "Shows game progress during some tests using \\r." ON)
IF(TEST_SHOW_GAME_PROGRESS)
    ADD_DEFINITIONS(-DTEST_SHOW_GAME_PROGRESS)
ENDIF(TEST_SHOW_GAME_PROGRESS)

include(FetchContent)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
  GIT_TAG v3.9.1)

FetchContent_MakeAvailable(json)
include_directories(${json_SOURCE_DIR}/include/)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v2.13.7)

FetchContent_MakeAvailable(Catch2)
include_directories(${Catch2_SOURCE_DIR}/include/)

# Main

file(GLOB source_files
    "src/agents/*.cpp"
    "src/bboard/*.cpp"
    "src/pyinterface/*.cpp"
    "src/*.cpp"
)

include_directories("include/")
add_executable("${PROJECT_NAME}_bin" ${source_files})
set_target_properties("${PROJECT_NAME}_bin"
        PROPERTIES OUTPUT_NAME ${PROJECT_NAME})

target_link_libraries("${PROJECT_NAME}_bin" Threads::Threads)
target_link_libraries("${PROJECT_NAME}_bin" nlohmann_json::nlohmann_json)

# Lib

set(source_files_nomain ${source_files})
# remove main.cpp from source files
list(FILTER source_files_nomain EXCLUDE REGEX "main.cpp$")

add_library("${PROJECT_NAME}_lib" SHARED ${source_files_nomain})
set_target_properties("${PROJECT_NAME}_lib"
        PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
        
target_link_libraries("${PROJECT_NAME}_lib" Threads::Threads)
target_link_libraries("${PROJECT_NAME}_lib" nlohmann_json::nlohmann_json)

# Test

file(GLOB test_files
    "unit_test/bboard/*.cpp"
    "unit_test/pyinterface/*.cpp"
    "unit_test/*.cpp"
)

add_executable("${PROJECT_NAME}_test" ${test_files} ${source_files_nomain})
target_include_directories("${PROJECT_NAME}_test" PUBLIC "unit_test/")
target_link_libraries("${PROJECT_NAME}_test" Threads::Threads)
target_link_libraries("${PROJECT_NAME}_test" nlohmann_json::nlohmann_json Catch2::Catch2)
