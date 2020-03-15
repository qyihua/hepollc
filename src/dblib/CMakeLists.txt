cmake_minimum_required (VERSION 2.6)
project (db C)


add_compile_options(-fPIC)



find_library(OCCI_LIBRARY occi
                NAMES occi
                PATHS /usr/local/lib /opt/instantclient
                )
find_library(CLNTSH_LIBRARY clntsh
                NAMES clntsh
                PATHS /usr/local/lib /opt/instantclient
                )


find_library(PG_LIBRARY
                NAMES pq
                PATHS /usr/local/lib /opt/PostgreSQL/pg/lib64
                )
                
find_path(PG_INCLUDES libpq-fe.h
                /usr/local/include
                /usr/include
                /opt/PostgreSQL/pg/include
                )
                
include_directories(${PG_INCLUDES} /opt/instantclient/include)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/../../../bin/dblib)
ADD_LIBRARY(dbpg SHARED db_pg.c)
ADD_LIBRARY(dboci SHARED db_oci.c)
target_link_libraries(dbpg dl pthread ${PG_LIBRARY})
target_link_libraries(dboci dl pthread ${CLNTSH_LIBRARY} ${OCCI_LIBRARY})
