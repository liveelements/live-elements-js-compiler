cmake_minimum_required(VERSION 3.11)

project (live_elements_js_compiler)
set(CMAKE_CXX_STANDARD 17)

add_library(live_elements_js_compiler SHARED src/compilerwrap.cpp ${CMAKE_JS_SRC})

include(${CMAKE_CURRENT_SOURCE_DIR}/project/functions.cmake)

# Configuration Options

option(BUILD_TESTS "Build tests."  ON)


# Configuragion Log

message("\nBuild Configuration:")
message("  * BUILD_TESTS:             ${BUILD_TESTS}")
message("")

# Include catch

if(BUILD_TESTS)
    set(CATCH2_AMALGAMATED_PATH "${CMAKE_CURRENT_SOURCE_DIR}/test/3rdparty/catch2/extras")
endif()

# Add libraries

add_subdirectory(lib)

target_compile_definitions(live_elements_js_compiler PRIVATE
    LV_BASE_STATIC 
    LV_ELEMENTS_COMPILER_STATIC
)
target_link_libraries(live_elements_js_compiler PRIVATE
    lvbase
    lvelementscompiler
)

# Add CMake-js

include_directories(${CMAKE_JS_INC})
target_link_libraries(${PROJECT_NAME} PUBLIC ${CMAKE_JS_LIB})

# Add napi

execute_process(COMMAND node -p "require('node-addon-api').include"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE NODE_ADDON_API_DIR
)
string(REPLACE "\n" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})
string(REPLACE "\"" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

set_target_properties(live_elements_js_compiler PROPERTIES PREFIX "" SUFFIX ".node")

target_include_directories(live_elements_js_compiler PRIVATE 
    "${NODE_ADDON_API_DIR}"
) 
target_compile_definitions(live_elements_js_compiler PRIVATE
    LV_BASE_STATIC 
    LV_ELEMENTS_COMPILER_STATIC
    NAPI_CPP_EXCEPTIONS
)

if (WIN32) 
    target_compile_definitions(live_elements_js_compiler PRIVATE
        _HAS_EXCEPTIONS=1
    )
endif()