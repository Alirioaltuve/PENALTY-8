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
    test_arbitro
    testbench_integracion
)

foreach(tb ${TESTBENCHES})
    add_executable(${tb} ${tb}.cpp)
    target_link_libraries(${tb} PRIVATE ${SC_LIBS})
endforeach()

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
add_test(NAME arbitro                    COMMAND test_arbitro)
add_test(NAME integracion                COMMAND testbench_integracion)
