cmake_minimum_required (VERSION 2.6)
project (staticfile C)

add_compile_options(-fPIC)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../../../../bin/app)
ADD_LIBRARY(staticfile SHARED app.c handle.c)
