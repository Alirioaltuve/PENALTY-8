cmake_minimum_required(VERSION 3.16)
project(Penalty8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- SystemC ----------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMC REQUIRED systemc)
find_package(Threads REQUIRED)

include_directories(${SYSTEMC_INCLUDE_DIRS})
link_directories(${SYSTEMC_LIBRARY_DIRS})

set(SC_LIBS ${SYSTEMC_LIBRARIES} Threads::Threads)

# ---------------- Ejecutables de prueba ----------------
# Cada archivo tiene su propio sc_main, por eso es un ejecutable.
set(TESTBENCHES
    testbench
    testbench_alu
    testbench_pc
    testbench_memoria_instrucciones
    test_unidad_control
    test_memoria_datos
    test_registro_banderas
    test_cpu_goalkeeper
    test_pipeline
    test_forwarding
    test_hazard
    test_branch
    test_communication
    test_cpu_io
    test_goalkeeper_strategy
    test_fase6
    test_arbitro
    testbench_integracion
    test_core_extra
    test_assembler
    test_acceptance
)

foreach(tb ${TESTBENCHES})
    add_executable(${tb} ${tb}.cpp)
    target_link_libraries(${tb} PRIVATE ${SC_LIBS})
endforeach()

# Demostracion interactiva por terminal (no forma parte de CTest).
add_executable(demo_goalkeeper demo_goalkeeper.cpp)
target_link_libraries(demo_goalkeeper PRIVATE ${SC_LIBS})

# Evaluador de programas .asm (no forma parte de CTest).
add_executable(eval_goalkeeper eval_goalkeeper.cpp)
target_link_libraries(eval_goalkeeper PRIVATE ${SC_LIBS})

enable_testing()

# Pruebas que devuelven 0 cuando todas sus comprobaciones pasan.
add_test(NAME banco_registros            COMMAND testbench)
add_test(NAME alu                        COMMAND testbench_alu)
add_test(NAME pc                         COMMAND testbench_pc)
add_test(NAME memoria_instrucciones      COMMAND testbench_memoria_instrucciones)
add_test(NAME unidad_control             COMMAND test_unidad_control)
add_test(NAME memoria_datos              COMMAND test_memoria_datos)
add_test(NAME registro_banderas          COMMAND test_registro_banderas)
add_test(NAME cpu_goalkeeper             COMMAND test_cpu_goalkeeper)
add_test(NAME pipeline                   COMMAND test_pipeline)
add_test(NAME forwarding                 COMMAND test_forwarding)
add_test(NAME hazard                     COMMAND test_hazard)
add_test(NAME branch                     COMMAND test_branch)
add_test(NAME communication              COMMAND test_communication)
add_test(NAME cpu_io                     COMMAND test_cpu_io)
add_test(NAME goalkeeper_strategy        COMMAND test_goalkeeper_strategy)
add_test(NAME fase6                      COMMAND test_fase6)
add_test(NAME arbitro                    COMMAND test_arbitro)
add_test(NAME integracion                COMMAND testbench_integracion)
add_test(NAME core_extra                 COMMAND test_core_extra)
add_test(NAME assembler                  COMMAND test_assembler)
add_test(NAME acceptance                 COMMAND test_acceptance)
