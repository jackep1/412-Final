cmake_minimum_required(VERSION 3.30)
project(Version1)

set(CMAKE_CXX_STANDARD 20)

include_directories(.)

add_executable(Version1
    gl_frontEnd.cpp
    gl_frontEnd.h
    glPlatform.h
    main.cpp
    typesAndConstants.h
        gridTools.cpp
        gridTools.h)
