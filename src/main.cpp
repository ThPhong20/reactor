#ifdef REACTOR_HARDWARE_TEST
#include "application/hardware_test/hardware_test.h"
#else
#include "application/app.h"
#endif

// Enter the application initialization and continuous monitoring loop.
extern "C" void app_main(void)
{
#ifdef REACTOR_HARDWARE_TEST
    app::hardware_test::run();
#else
    app::init();
    app::run();
#endif
}
