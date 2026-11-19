cmake_minimum_required(VERSION 3.10)
project(roomba_mqtt CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(MOSQUITTO REQUIRED libmosquitto)
find_package(Threads REQUIRED)

add_executable(roomba_mqtt
  src/main.cpp
  src/SerialPort.cpp
  src/Roomba.cpp
  src/SensorData.cpp
  src/MQTTClient.cpp
  src/CommandHandler.cpp
)

target_include_directories(roomba_mqtt PRIVATE include ${MOSQUITTO_INCLUDE_DIRS})
target_link_libraries(roomba_mqtt PRIVATE ${MOSQUITTO_LIBRARIES} Threads::Threads)
target_compile_options(roomba_mqtt PRIVATE -Wall -Wextra)