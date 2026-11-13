# Register the union of dependencies for ESP-IDF's early requirements pass.
# Only the selected entry point is called; test-specific sources are conditional.
set(hardware_test_sources)
if(REACTOR_HARDWARE_TEST)
    list(APPEND hardware_test_sources
        "../src/application/hardware_test/controller.cpp"
        "../src/application/hardware_test/session.cpp"
        "../src/application/hardware_test/hardware_test.cpp"
        "../src/hal/test_console/test_console.cpp")
endif()

idf_component_register(
    SRCS
        ${hardware_test_sources}
        "../src/main.cpp"
        "../src/application/app.cpp"
        "../src/application/state_machine/state_machine.cpp"
        "../src/application/data_store/data_store.cpp"
        "../src/application/display_manager/display_manager.cpp"
        "../src/common/system_config.cpp"
        "../src/application/scheduler/scheduler.cpp"
        "../src/application/sensor_manager/sensor_manager.cpp"
        "../src/application/pump_manager/pump_manager.cpp"
        "../src/application/telemetry/telemetry.cpp"
        "../src/driver/pump_pwm/pump_pwm.cpp"
        "../src/driver/ph/ph.cpp"
        "../src/driver/ec/ec.cpp"
        "../src/driver/turbidity/turbidity.cpp"
        "../src/application/turbidity_manager/turbidity_manager.cpp"
        "../src/driver/serial/serial.cpp"
        "../src/driver/tft/tft.cpp"
        "../src/hal/timers_watchdog/timers_watchdog.cpp"
        "../src/hal/pwm/pwm.cpp"
        "../src/hal/adc/adc.cpp"
        "../src/hal/serial/serial.cpp"
        "../src/hal/lcd/lcd.cpp"
    INCLUDE_DIRS "../src"
    PRIV_REQUIRES
        esp_adc
        esp_lcd
        esp_driver_spi
        heap
        esp_driver_gpio
        esp_driver_ledc
        esp_driver_uart
        esp_driver_usb_serial_jtag
        esp_timer
        esp_system
        freertos
        log
)

if(REACTOR_HARDWARE_TEST)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE REACTOR_HARDWARE_TEST=1)
endif()
