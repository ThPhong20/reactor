cmake_minimum_required(VERSION 3.22)
project(reactor_host_tests LANGUAGES CXX)

enable_testing()
add_executable(pump_control_test
    pump_control_test.cpp
    ../application/pump_manager/pump_manager.cpp
    ../driver/pump_pwm/pump_pwm.cpp
    ../common/system_config.cpp
)
target_include_directories(pump_control_test PRIVATE ..)
target_compile_features(pump_control_test PRIVATE cxx_std_17)
target_compile_options(pump_control_test PRIVATE -Wall -Wextra -Werror)

foreach(scenario healthy timer_failure init_failure_0 init_failure_1 init_failure_2
        write_failure_0 write_failure_1 write_failure_2 stop_failure reinit duplicate_pins)
    add_test(NAME pump_${scenario} COMMAND pump_control_test ${scenario})
endforeach()

add_executable(sensor_test
    sensor_test.cpp
    ../application/sensor_manager/sensor_manager.cpp
    ../application/telemetry/telemetry.cpp
    ../driver/ph/ph.cpp
    ../driver/ec/ec.cpp
    ../common/system_config.cpp
)
target_include_directories(sensor_test PRIVATE ..)
target_compile_features(sensor_test PRIVATE cxx_std_17)
target_compile_options(sensor_test PRIVATE -Wall -Wextra -Werror)
foreach(scenario conversion calibration temperature invalid_readings partial_init pin_conflict telemetry invalid_output)
    add_test(NAME sensor_${scenario} COMMAND sensor_test ${scenario})
endforeach()

add_executable(display_test
    display_test.cpp
    ../application/data_store/data_store.cpp
    ../application/display_manager/display_manager.cpp
    ../driver/tft/tft.cpp
    ../common/system_config.cpp
)
target_include_directories(display_test PRIVATE ..)
target_compile_features(display_test PRIVATE cxx_std_17)
target_compile_options(display_test PRIVATE -Wall -Wextra -Werror)
foreach(scenario storage format pins init_failure transfer_failure enable_failure render)
    add_test(NAME display_${scenario} COMMAND display_test ${scenario})
endforeach()
