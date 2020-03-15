cmake_minimum_required (VERSION 3.5)
project (HepollCServer C)

SET(CMAKE_C_FLAGS "$ENV{CFLAGS} ")

SET(CMAKE_C_FLAGS_DEBUG "$ENV{CFLAGS} -ggdb  -O0 -g -fno-omit-frame-pointer")
SET(CMAKE_C_FLAGS_RELEASE "$ENV{CFLAGS} -O3 ")

add_subdirectory(app/staticfile)
add_subdirectory(app/helloworld)

#add_subdirectory(dblib)

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/com com)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/coroutine coroutine)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/http http)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/util util)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/database database)

include_directories(.)

add_executable(hepollc main.c ${coroutine} ${http} ${com} ${util} ${database})

SET_TARGET_PROPERTIES(hepollc PROPERTIES ENABLE_EXPORTS TRUE)
set_target_properties(hepollc PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

target_link_libraries(hepollc ssl crypto dl pthread )
