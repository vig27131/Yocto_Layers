cmake_minimum_required(VERSION 3.10)

project(Quantum_systems)

add_executable(eventloop_revision eventloop_revision.cpp)

# Required — links pthread for std::thread
find_package(Threads REQUIRED)
target_link_libraries(eventloop_revision Threads::Threads)

# Required — installs binary to /usr/bin on Pi
install(TARGETS eventloop_revision
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
