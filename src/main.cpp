#include "application/app.h"

extern "C" void app_main(void)
{
    app::init();
    app::run();
}
