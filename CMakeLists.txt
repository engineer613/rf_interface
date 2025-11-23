cmake_minimum_required(VERSION 3.10)
project(RFInterface)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add compiler warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

# Find required packages
find_package(Threads REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Create library
add_library(rfinterface STATIC
    src/RFInterface.cpp
    src/RFInterface.h
)

# Link pthread for threading support
target_link_libraries(rfinterface PRIVATE Threads::Threads)

# Create executable
add_executable(rf_test src/main.cpp)

# Link the library to the executable
target_link_libraries(rf_test rfinterface Threads::Threads)

# Installation rules (optional)
install(TARGETS rfinterface DESTINATION lib)
install(TARGETS rf_test DESTINATION bin)
install(FILES RFInterface.h DESTINATION include)
