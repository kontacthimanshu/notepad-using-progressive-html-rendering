// CRT leak-check listener (Constitution XI "Resource cleanup").
// Debug builds only: snapshots the CRT heap at each test case start and compares at its end.
// A test that leaks normal blocks makes the test executable exit non-zero (CTest runs each
// test case as its own process, so the leaking case fails). Opt out with
// `* doctest::description("leakcheck:off")`.
//
// Tolerance: one-time lazy statics (doctest capture buffers, iostream locale facets, filesystem
// caches) persist after their first use and appear as a handful of tiny "leaked" blocks. A test
// case therefore fails only above kToleranceBytes / kToleranceBlocks; genuine per-operation leaks
// are caught by tests/integration/test_resource_cleanup.cpp, which repeats operations 1000 times.

#include <cstdio>
#include <cstring>
#include <string>

#include <doctest.h>

#ifdef _DEBUG
#include <crtdbg.h>
#endif

namespace pxc::test {

int g_leakFailures = 0;
constexpr std::size_t kToleranceBytes = 4096;
constexpr std::size_t kToleranceBlocks = 32;

void linkLeakCheck() {} // referenced from main() so this object file is always linked

#ifdef _DEBUG
namespace {

struct LeakCheckListener : doctest::IReporter {
    explicit LeakCheckListener(const doctest::ContextOptions&) {}

    void report_query(const doctest::QueryData&) override {}
    void test_run_start() override {}
    void test_run_end(const doctest::TestRunStats&) override {}
    void test_case_start(const doctest::TestCaseData& tc) override {
        enabled_ = !(tc.m_description && std::strstr(tc.m_description, "leakcheck:off"));
        _CrtMemCheckpoint(&start_);
    }
    void test_case_reenter(const doctest::TestCaseData&) override {}
    void test_case_end(const doctest::CurrentTestCaseStats&) override {
        if (!enabled_) return;
        _CrtMemState end{}, diff{};
        _CrtMemCheckpoint(&end);
        if (_CrtMemDifference(&diff, &start_, &end) &&
            (diff.lSizes[_NORMAL_BLOCK] > kToleranceBytes || diff.lCounts[_NORMAL_BLOCK] > kToleranceBlocks)) {
            ++g_leakFailures;
            std::fprintf(stderr, "LEAK CHECK FAILED: %zu normal block(s), %zu byte(s) not freed\n",
                         static_cast<std::size_t>(diff.lCounts[_NORMAL_BLOCK]),
                         static_cast<std::size_t>(diff.lSizes[_NORMAL_BLOCK]));
            _CrtMemDumpStatistics(&diff);
        }
    }
    void test_case_exception(const doctest::TestCaseException&) override {}
    void subcase_start(const doctest::SubcaseSignature&) override {}
    void subcase_end() override {}
    void log_assert(const doctest::AssertData&) override {}
    void log_message(const doctest::MessageData&) override {}
    void test_case_skipped(const doctest::TestCaseData&) override {}

private:
    _CrtMemState start_{};
    bool enabled_ = true;
};

} // namespace

REGISTER_LISTENER("leakcheck", 1, LeakCheckListener);
#endif

} // namespace pxc::test
