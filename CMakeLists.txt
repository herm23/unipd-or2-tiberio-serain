cmake_minimum_required(VERSION 3.20)
project(unipd_or2_tiberio_serain C)

set(CMAKE_C_STANDARD 11)

add_executable(unipd_or2_tiberio_serain
        main.c
        src/tsp.c
        src/chrono.c
)

# Path CPLEX configurabile
set(CPLEX_ROOT "" CACHE PATH "Path to CPLEX installation")

if(CPLEX_ROOT)

    target_include_directories(unipd_or2_tiberio_serain PRIVATE
            ${CPLEX_ROOT}/cplex/include
            ${CPLEX_ROOT}/cplex/include/ilcplex
    )

    target_compile_definitions(unipd_or2_tiberio_serain PRIVATE IL_STD)

    # WINDOWS
    if(WIN32)

        target_link_directories(unipd_or2_tiberio_serain PRIVATE
                ${CPLEX_ROOT}/cplex/lib/x64_windows_msvc14/stat_mda
        )

        target_link_libraries(unipd_or2_tiberio_serain PRIVATE
                cplex2212
                ilocplex
        )

    endif()

    # LINUX
    if(UNIX AND NOT APPLE)

        target_link_directories(unipd_or2_tiberio_serain PRIVATE
                ${CPLEX_ROOT}/cplex/lib/x86-64_linux/static_pic
        )

        target_link_libraries(unipd_or2_tiberio_serain PRIVATE
                cplex
                ilocplex
                pthread
                m
                dl
        )

    endif()

    # macOS
    if(APPLE)

        target_link_directories(unipd_or2_tiberio_serain PRIVATE
                ${CPLEX_ROOT}/cplex/lib/arm64_osx/static_pic
        )

        target_link_libraries(unipd_or2_tiberio_serain PRIVATE
                cplex
                ilocplex
                pthread
                m
        )

    endif()

endif()