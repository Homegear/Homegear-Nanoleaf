cmake_minimum_required(VERSION 3.8)
project(homegear_nanoleaf)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        src/PhysicalInterfaces/INanoleafInterface.cpp
        src/PhysicalInterfaces/INanoleafInterface.h
        src/PhysicalInterfaces/NanoleafInterface.cpp
        src/PhysicalInterfaces/NanoleafInterface.h
        src/Factory.cpp
        src/Factory.h
        src/GD.cpp
        src/GD.h
        src/Nanoleaf.cpp
        src/Nanoleaf.h
        src/NanoleafCentral.cpp
        src/NanoleafCentral.h
        src/NanoleafPacket.cpp
        src/NanoleafPacket.h
        src/NanoleafPeer.cpp
        src/NanoleafPeer.h
        src/PacketManager.cpp
        src/PacketManager.h
        config.h)

add_custom_target(homegear COMMAND ../../makeAll.sh SOURCES ${SOURCE_FILES})

add_library(homegear_nanoleaf ${SOURCE_FILES})