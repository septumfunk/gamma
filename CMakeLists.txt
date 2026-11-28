cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(LIBRARY_TYPE STATIC)
set(ROOT ${CMAKE_CURRENT_SOURCE_DIR})

project(gamma C)

add_compile_options(
  -Wall -Wextra -Werror
  -Wpedantic -Wconversion -Wuninitialized
  -Wsign-conversion -Wnull-dereference
  $<$<CONFIG:Release>:-O3>
)

# Library
add_library(${PROJECT_NAME} ${LIBRARY_TYPE}
  src/render.c
  src/scene.c
)
target_include_directories(${PROJECT_NAME} PUBLIC ${ROOT}/include)

if (PROJECT_IS_TOP_LEVEL)
  find_package(SDL2 CONFIG REQUIRED)
  find_package(raylib CONFIG REQUIRED)
  include(FetchContent)
  FetchContent_Declare(
    sf-std
    GIT_REPOSITORY https://github.com/septumfunk/sf-std.git
    GIT_TAG master
  )
  FetchContent_MakeAvailable(sf-std)

  # Tests
  add_executable(gamma_test
    tests/main.c
    tests/sdl.c
    tests/raylib.c
    tests/minecraft.c
  )
  target_include_directories(gamma_test PUBLIC ${ROOT}/include)
  target_link_libraries(gamma_test PUBLIC gamma SDL2::SDL2 raylib sf-std)
endif()

# Sanitizer suite
option(ENABLE_SANITIZERS "Enable sanitizers" OFF)
if (ENABLE_SANITIZERS)
  message(STATUS "Sanitizers enabled")
  set(SANITIZER_COMPILE_FLAGS
    -fsanitize=address,undefined
    -fno-omit-frame-pointer
    -g
    -O0
  )

  target_compile_options(sf-std PRIVATE ${SANITIZER_COMPILE_FLAGS})
  target_compile_options(gamma PRIVATE ${SANITIZER_COMPILE_FLAGS})
  target_compile_options(gamma_test PRIVATE ${SANITIZER_COMPILE_FLAGS})
endif()
