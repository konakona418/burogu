cmake_minimum_required(VERSION 3.10.0)
project(burogu VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)

set(CLAY_INCLUDE_ALL_EXAMPLES OFF)
add_subdirectory(vendors/clay)
add_subdirectory(vendors/cmark)
add_subdirectory(vendors/raylib)

if (EMSCRIPTEN)
    message(STATUS "Configuring for Emscripten/WebAssembly target")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")
endif()

add_executable(burogu main.c clay_impl.c font_loader.c)
target_include_directories(burogu PRIVATE vendors/clay vendors/cmark/src ${CMAKE_BINARY_DIR}/vendors/cmark/src)
target_link_libraries(burogu PRIVATE cmark raylib)

if (EMSCRIPTEN)
target_link_options(burogu PRIVATE
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=67108864"
    "-sASSERTIONS=2"
    "-sEXPORTED_FUNCTIONS=['_main','_malloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=['UTF8ToString']"
    "--preload-file=${CMAKE_SOURCE_DIR}/markdown@/markdown"
)
endif()
