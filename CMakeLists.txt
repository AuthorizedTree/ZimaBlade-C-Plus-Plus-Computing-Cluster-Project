cmake_minimum_required(VERSION 3.16)
project(ZimaBladeDistributedCluster VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(cluster-controller controller/main.cpp)
target_link_libraries(cluster-controller PRIVATE Threads::Threads)

add_executable(cluster-worker worker/main.cpp)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(cluster-controller PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(cluster-worker PRIVATE -Wall -Wextra -Wpedantic)
endif()
