# Add libraries

set(BUILD_LVBASE_STATIC ON)
set(BUILD_LVELEMENTSCOMPILER_STATIC ON)

add_subdirectory(lvbase)
add_subdirectory(lvelements/compiler)

# Set position independent code for static libraries

if(UNIX AND NOT APPLE)
    set_property(TARGET lvbase PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET lvelementscompiler PROPERTY POSITION_INDEPENDENT_CODE ON)
endif()

