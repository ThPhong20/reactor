#include "application/app.h"

// Enter the application initialization and continuous monitoring loop.
extern "C" void app_main(void)
{
    app::init();
    app::run();
}
