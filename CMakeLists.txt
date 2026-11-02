cmake_minimum_required(VERSION 3.24)

project(Game710 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build SDL3 into the app instead of distributing a separate SDL3 library.
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

# SDL3 is already in this repository as a Git submodule.
add_subdirectory(external/SDL EXCLUDE_FROM_ALL)

# Include all .cpp files in the project's root directory.
file(GLOB GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

if(NOT GAME_SOURCES)
    message(FATAL_ERROR "No game .cpp files found in project root")
endif()

add_executable(game710 MACOSX_BUNDLE ${GAME_SOURCES})

set_target_properties(game710 PROPERTIES
    OUTPUT_NAME "710"
    MACOSX_BUNDLE_BUNDLE_NAME "710"
)

target_include_directories(game710 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(game710 PRIVATE SDL3::SDL3)

# SDL_GetBasePath() resolves to Contents/Resources for a macOS .app.
# Copy levels inside the bundle so the game's existing loader works.
add_custom_command(TARGET game710 POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:game710>/../Resources/assets"
    COMMENT "Copying game assets into 710.app"
    VERBATIM
)