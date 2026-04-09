cmake_minimum_required(VERSION 3.30)
project(Version4)

set(CMAKE_CXX_STANDARD 20)

include_directories(.)

add_executable(Version4
    gl_frontEnd.cpp
    gl_frontEnd.h
    glPlatform.h
    main.cpp
    typesAndConstants.h)
