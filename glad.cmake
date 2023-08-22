# -----------------------------------------------------------------
#
# GLAD
# ====
#
# GENERAL INFO:
#
#   The `GLAD` dependency includes a function loader for
#   OpenGL. In the future we want to add a GL function loader as
#   part of Trameleon which allows to add helpers for graphics
#   interop with the encoders and decoders. For now we use GLAD
#   as this saves us some time during development.
#
#
# USAGE:
#
#   - On ArchLinux make sure to run `sudo pacman -S python-jinja`
#   - Include `glad.cmake` into your `CMakeLists.txt`
#
# REFERENCES:
# 
#   [0]: https://github.com/Dav1dde/glad/wiki/C#cmake
#
# -----------------------------------------------------------------

include(FetchContent)

FetchContent_Declare(
  glad
  GIT_REPOSITORY https://github.com/Dav1dde/glad.git
  GIT_TAG v2.0.2
)

FetchContent_MakeAvailable(glad)

FetchContent_GetProperties(glad)

add_subdirectory("${glad_SOURCE_DIR}/cmake" glad_cmake)

glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)

list(APPEND tra_libs glad_gl_core_33)

# -----------------------------------------------------------------


