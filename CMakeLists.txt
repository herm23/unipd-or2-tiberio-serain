cmake_minimum_required(VERSION 4.0)
project(unipd_or2_tiberio_serain C)

set(CMAKE_C_STANDARD 11)

add_executable(unipd_or2_tiberio_serain main.c
        tsp.c
#        tsp.h
#        chrono.h
        chrono.c
)
