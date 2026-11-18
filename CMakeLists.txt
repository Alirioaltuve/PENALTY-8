cmake_minimum_required(VERSION 3.16)
project(GOALKEEPER-8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------- SystemC ----------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMC REQUIRED systemc)
find_package(Threads REQUIRED)

include_directories(${SYSTEMC_INCLUDE_DIRS})
link_directories(${SYSTEMC_LIBRARY_DIRS})

set(SC_LIBS ${SYSTEMC_LIBRARIES} Threads::Threads)

# ---------------- Demostracion individual de GOALKEEPER-8 ----------------
# Instancia la CPU real, el CommunicationMailbox y el Arbitro.
add_executable(demo_goalkeeper demo_goalkeeper.cpp)
target_link_libraries(demo_goalkeeper PRIVATE ${SC_LIBS})
