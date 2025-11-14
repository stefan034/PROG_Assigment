cmake_minimum_required(VERSION 3.5)
project(bme280_mqtt CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)


include_directories(${MOSQUITTO_INCLUDE_DIRS} include src)
link_directories(${MOSQUITTO_LIBRARY_DIRS})


add_executable(bme280_mqtt
src/main.cpp
src/BME280.cpp
src/mqtt_client.cpp
)


target_link_libraries(bme280_mqtt ${MOSQUITTO_LIBRARIES} pthread)


# If pkg-config didn't work on some systems, fall back to linking by name
if (NOT MOSQUITTO_LIBRARIES)
target_link_libraries(bme280_mqtt mosquitto pthread)
endif()