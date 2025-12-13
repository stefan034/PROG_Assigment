cmake_minimum_required(VERSION 3.12)
project(BME280_Project CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -----------------------------------------
# BME280 LIBRARY
# -----------------------------------------
add_library(bme280
    BME280.cpp
    BME280.hpp
)

target_include_directories(bme280 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# -----------------------------------------
# MAIN EXECUTABLE
# -----------------------------------------
add_executable(main_program
    main.cpp
)

target_link_libraries(main_program
    bme280
)

# -----------------------------------------
# UNIT TESTS
# -----------------------------------------
option(BUILD_TESTS "Enable unit tests" ON)

if (BUILD_TESTS)

    enable_testing()

    # ---- Fetch GoogleTest automatically ----
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    FetchContent_MakeAvailable(googletest)

    # -------------------------------------
    # Unit test executable
    # -------------------------------------
    add_executable(bme280_tests
        tests/test_BME280.cpp
    )

    target_link_libraries(bme280_tests
        bme280
        GTest::gtest
        GTest::gtest_main
        pthread
    )

    # Test build flags → enables your test code blocks
    target_compile_definitions(bme280_tests PRIVATE
        UNIT_TEST
        UNIT_TEST_FAKE_CAL
        UNIT_TEST_FAKE_MEASUREMENTS
    )

    add_test(NAME BME280_Tests COMMAND bme280_tests)

endif()
f