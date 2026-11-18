cmake_minimum_required(VERSION 3.16)
project(PENALTY-8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- SystemC ----------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMC REQUIRED systemc)
find_package(Threads REQUIRED)

include_directories(${SYSTEMC_INCLUDE_DIRS})
link_directories(${SYSTEMC_LIBRARY_DIRS})

set(SC_LIBS ${SYSTEMC_LIBRARIES} Threads::Threads)

# ---------------- Ejecutables de GOALKEEPER-8 ----------------
# 1. Demostracion interactiva del sistema de penaltis.
add_executable(demo_goalkeeper demo_goalkeeper.cpp)
target_link_libraries(demo_goalkeeper PRIVATE ${SC_LIBS})

# 2. Evaluador de programas RISC independientes (.asm).
add_executable(eval_goalkeeper eval_goalkeeper.cpp)
target_link_libraries(eval_goalkeeper PRIVATE ${SC_LIBS})
