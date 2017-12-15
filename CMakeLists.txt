cmake_minimum_required(VERSION 3.8)
project(homegear_nanoleaf)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        src/Factory.cpp
        src/Factory.h
        src/GD.cpp
        src/GD.h
        src/Nanoleaf.cpp
        src/Nanoleaf.h
        src/NanoleafCentral.cpp
        src/NanoleafCentral.h
        src/NanoleafPeer.cpp
        src/NanoleafPeer.h
        config.h)

add_custom_target(homegear COMMAND ../../makeAll.sh SOURCES ${SOURCE_FILES})

add_library(homegear_nanoleaf ${SOURCE_FILES})