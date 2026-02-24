file(GLOB plugin_dirs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*")
list(SORT plugin_dirs)

foreach(plugin_dir IN LISTS plugin_dirs)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${plugin_dir}" AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${plugin_dir}/CMakeLists.txt")
        add_subdirectory(${plugin_dir})
    endif()
endforeach()
