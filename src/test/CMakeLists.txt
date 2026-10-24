cmake_minimum_required(VERSION 3.22)
project(pump_control_tests LANGUAGES CXX)

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
