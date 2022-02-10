cmake_minimum_required(VERSION 3.1)

project (live_elements_js_compiler)
set(CMAKE_CXX_STANDARD 14)

add_library(live_elements_js_compiler SHARED src/compilerwrap.cpp ${CMAKE_JS_SRC})

include(./config/qt.cmake)
AddQtSupport(live_elements_js_compiler)

add_subdirectory(lib)


if (UNIX AND NOT APPLE)
    # Set the RPATH in the binary to a relative one to allow it to find our Qt
    # libraries regardless of where the base installation directory is.
    file(RELATIVE_PATH QT_LIBRARY_REL_PATH "${CMAKE_BINARY_DIR}/Release" "${QT_CMAKE_HOME_DIR}/../..")
    set_target_properties(live_elements_js_compiler PROPERTIES INSTALL_RPATH "\$ORIGIN/${QT_LIBRARY_REL_PATH}")
    set_target_properties(live_elements_js_compiler PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
endif()

if (APPLE)
    file(RELATIVE_PATH QT_LIBRARY_REL_PATH "${CMAKE_BINARY_DIR}/Release" "${QT_CMAKE_HOME_DIR}/../..")
    set_target_properties(live_elements_js_compiler PROPERTIES INSTALL_RPATH "@loader_path/${QT_LIBRARY_REL_PATH}")
    set_target_properties(live_elements_js_compiler PROPERTIES SKIP_BUILD_RPATH FALSE)
    set_target_properties(live_elements_js_compiler PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE)
endif()


target_link_libraries(live_elements_js_compiler PRIVATE
    Qt5::Core
    lvbase
    lvelements
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
    NAPI_CPP_EXCEPTIONS
)

if (WIN32) 
    target_compile_definitions(live_elements_js_compiler PRIVATE
        _HAS_EXCEPTIONS=1
    )
endif()