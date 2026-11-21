// Shared doctest entry point for every PixelCSS test executable.
// A custom main() references the leak-check object so its listener is always linked in.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>

namespace pxc::test {
extern int g_leakFailures;
void linkLeakCheck();
} // namespace pxc::test

int main(int argc, char** argv) {
    pxc::test::linkLeakCheck();
    doctest::Context context(argc, argv);
    const int rc = context.run();
    if (context.shouldExit()) return rc;
    if (rc == 0 && pxc::test::g_leakFailures > 0) return 3;
    return rc;
}
