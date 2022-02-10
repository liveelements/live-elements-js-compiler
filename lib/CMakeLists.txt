include(../config/qt.cmake)

## lvbase library
## ------------------------------------------------------------------------

add_library(lvbase STATIC  
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/applicationcontext.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/commandlineparser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/exception.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/fileio.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/library.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/libraryloadpath.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/librarytable.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/lockedfileiosession.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/mlnode.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/mlnodetojson.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/module.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/package.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/packagegraph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/palettecontainer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/program.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/stacktrace.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/typename.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/utf8.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/version.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/visuallog.cpp"
)

if (WIN32)
    target_sources(lvbase
        PRIVATE 
            "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/stacktrace_win.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/libraryloadpath_win.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/applicationcontext_win.cpp"
    )
endif ()

if(UNIX AND NOT APPLE)
    target_sources(lvbase
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/stacktrace_unix.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/libraryloadpath_unix.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/applicationcontext_unix.cpp"
    )
endif()

if(APPLE)
    target_sources(lvbase
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/stacktrace_unix.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/libraryloadpath_unix.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src/applicationcontext_mac.cpp"
    )
endif()

target_include_directories(lvbase
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/src"
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/include"
)
target_compile_definitions(lvbase PRIVATE LV_BASE_LIB LV_BASE_STATIC)

# Link qt

AddQtSupport(lvbase)
target_link_libraries(lvbase PRIVATE Qt5::Core)

# Include 3rdparty library - backward

if(UNIX)
    target_include_directories(lvbase
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/3rdparty/backward"
    )
endif()

# Include 3rdparty library - rapidjson

target_include_directories(lvbase
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/3rdparty/rapidjson/include"
)

# Include 3rdparty library - utf8proc

target_compile_definitions(lvbase PRIVATE UTF8PROC_EXPORTS)
target_include_directories(lvbase
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/3rdparty/utf8proc"
)
target_sources(lvbase
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvbase/3rdparty/utf8proc/utf8proc.c"
)

## lvelements/compiler library
## ------------------------------------------------------------------------

add_library(lvelements STATIC 
    "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/src/compiler/compiler.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/src/compiler/elementsmodule.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/src/compiler/elementssections.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/src/compiler/languagenodes.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/src/compiler/languageparser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/src/compiler/modulefile.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/src/compiler/sourcerange.cpp"
)
target_include_directories(lvelements
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/src"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/src/compiler"
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/include"
)
target_compile_definitions(lvelements PRIVATE LV_ELEMENTS_LIB LV_BASE_STATIC)

# Include 3rdparty library - treesitter

target_sources(lvelements
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/3rdparty/treesitter/lib/src/lib.c"
)
target_include_directories(lvelements
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/3rdparty/treesitter/lib/src"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/3rdparty/treesitter/lib/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/3rdparty/treesitter/utf8proc"
)

# Include library - tree-sitter-elements

target_sources(lvelements
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/3rdparty/treesitterelements/elementsparserinternal.c"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/3rdparty/treesitterelements/scanner.c"
)

target_include_directories(lvelements
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/lvelements/3rdparty/treesitterelements"
)

# Link qt and lvbase

AddQtSupport(lvelements)
target_link_libraries(lvelements PRIVATE Qt5::Core lvbase)

