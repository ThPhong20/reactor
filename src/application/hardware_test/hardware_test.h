#pragma once

namespace app::hardware_test {
// Run integrated monitoring and native-USB timed pump tests; never enables electrode power.
[[noreturn]] void run();
} // namespace app::hardware_test
