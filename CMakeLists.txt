cmake_minimum_required(VERSION 4.0)
project(unipd_or2_tiberio_serain C)

set(CMAKE_C_STANDARD 11)
set(CPLEX_ROOT "C:\\Program Files\\IBM\\ILOG\\CPLEX_Studio_Community2212")

add_executable(unipd_or2_tiberio_serain main.c
        tsp.c
#        tsp.h
#        chrono.h
        chrono.c
)

# Include directories
target_include_directories(unipd_or2_tiberio_serain PRIVATE
        ${CPLEX_ROOT}/cplex/include
        ${CPLEX_ROOT}/cplex/include/ilcplex
)

# Preprocessor definition
target_compile_definitions(unipd_or2_tiberio_serain PRIVATE IL_STD)

# Compile as C
set_target_properties(unipd_or2_tiberio_serain PROPERTIES LINKER_LANGUAGE C)

# MSVC specific options
if(MSVC)
    target_compile_options(unipd_or2_tiberio_serain PRIVATE
            /Gz
            /TC
            /wd4996
    )

    set_property(TARGET unipd_or2_tiberio_serain
            PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )

    target_link_directories(unipd_or2_tiberio_serain PRIVATE
            ${CPLEX_ROOT}/cplex/lib/x64_windows_msvc14/stat_mda
    )

    target_link_libraries(unipd_or2_tiberio_serain PRIVATE
            cplex2212
            ilocplex
#            concert
    )
endif()