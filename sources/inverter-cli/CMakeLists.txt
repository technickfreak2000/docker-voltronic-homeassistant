CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
set(CMAKE_CXX_STANDARD 17)

PROJECT("inverter_poller")

set (CMAKE_CXX_FLAGS "-O2 --std=c++0x ${CMAKE_CXX_FLAGS}")

INCLUDE_DIRECTORIES(lib/cJSON)
add_library(cJSON lib/cJSON/cJSON.c)

file(GLOB SOURCES *.cpp)
ADD_EXECUTABLE(inverter_poller ${SOURCES})
target_link_libraries(inverter_poller cJSON libpaho-mqttpp3.so libpaho-mqtt3as.so.1 -lpthread)
