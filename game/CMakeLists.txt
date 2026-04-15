cmake_minimum_required(VERSION 3.15)
project(ggpo_raylib_game C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# Raylib
# ---------------------------------------------------------------------------
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.0
    GIT_SHALLOW    TRUE
)
# Disable raylib's own examples/games to keep the build fast.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# ---------------------------------------------------------------------------
# GGPO – only build the library subdirectory (src/lib) to avoid pulling in
# the sample apps that require SDL2.
# ---------------------------------------------------------------------------
FetchContent_Declare(
    ggpo
    GIT_REPOSITORY https://github.com/pond3r/ggpo.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(ggpo)
if(NOT ggpo_POPULATED)
    FetchContent_Populate(ggpo)
    # Point CMake at the library-only subdirectory, not the root which also
    # tries to build the vectorwar / spectator sample applications.
    add_subdirectory(
        ${ggpo_SOURCE_DIR}/src/lib
        ${ggpo_BINARY_DIR}/ggpo-lib
        EXCLUDE_FROM_ALL
    )
endif()

# ---------------------------------------------------------------------------
# Game executable
# ---------------------------------------------------------------------------
add_executable(game
    src/main.c
    src/game.c
    src/input.c
    src/net.cpp      # must be C++ because ggponet.h is a C++ header
)

# -ffp-contract=off prevents the compiler from fusing multiply-add operations
# across translation units in ways that differ between platforms.  Deterministic
# floating-point is *required* for GGPO rollback to work correctly.
target_compile_options(game PRIVATE -ffp-contract=off)

target_include_directories(game PRIVATE
    src
    ${ggpo_SOURCE_DIR}/src/include
)

# GGPO's CMakeLists.txt names the target "GGPO" (all caps).
# If your copy of the library uses a different name, adjust here.
target_link_libraries(game PRIVATE
    raylib
    GGPO
)
