// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Shared test header for testing colors
 *
 * Copyright (C) 2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <cmath>
#include <iomanip>
#include <gtest/gtest.h>

namespace {

/**
 * Allow the correct tracing of the file and line where data came from when using P_TESTs.
 */
struct traced_data
{
    const char *_file;
    const int _line;

    ::testing::ScopedTrace enable_scope() const { return ::testing::ScopedTrace(_file, _line, ""); }
};
// Macro for the above tracing in P Tests
#define _P(type, ...)                   \
    type                                \
    {                                   \
        __FILE__, __LINE__, __VA_ARGS__ \
    }


/**
 * Print a vector of doubles for debugging
 */
std::string print_values(const std::vector<double> &v) 
{
    std::ostringstream oo; 
    oo << "{";
    bool first = true;
    for (double const &item : v) {
        if (!first) {
            oo << ", ";
        }   
        first = false;
        oo << std::setprecision(3) << item;
    }   
    oo << "}";
    return oo.str();
}

::testing::AssertionResult IsNear(double a, double b, double epsilon = 0.01)
{
    if (std::fabs(a - b) < epsilon) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure();
}

/**
 * Test each value in a values list is within a certain distance from each other.
 */
::testing::AssertionResult VectorIsNear(std::vector<double> const &A, std::vector<double> const &B, double epsilon)
{
    bool is_same = A.size() == B.size();
    for (size_t i = 0; is_same && i < A.size(); i++) {
        is_same = is_same and (std::fabs((A[i]) - (B[i])) < epsilon);
    }
    if (!is_same) {
        return ::testing::AssertionFailure() << "\n" << print_values(A) << "\n != \n" << print_values(B);
    }
    return ::testing::AssertionSuccess();
}


/**
 * Generate a count of random doubles between 0 and 1.
 *
 * Randomly appends an extra value for optional opacity.
 */
inline static std::vector<double> random_values(unsigned ccount)
{
    std::vector<double> values;
    for (unsigned j = 0; j < ccount; j++) {
        values.emplace_back(static_cast<double>(std::rand()) / RAND_MAX);
    }
    // randomly add opacity
    if (std::rand() > (RAND_MAX / 2)) {
        values.emplace_back(static_cast<double>(std::rand()) / RAND_MAX);
    }
    return values;
}

/** Locale testing fixture.
 *
 * This fixture only creates a locale, it is test responsibility to use it.
 * \code{.cpp}
 * class FooTestFixture : public LocaleTestFixture {};
 * TEST_P(FooTestFixture, Test1)
 * {
 *    std::sstream stream("1.32");
 *    stream.imbue(locale);
 *    float v;
 *    stream >> v;
 * }
 * TEST_P(FooTestFixture, Test2) { .. }
 * INSTANTIATE_TEST_SUITE_P(TestSuite,
 *                           FooTestFixture,
 *                           testing::Values("C", "de_DE.UTF8"));
 * \endcode
 */
class LocaleTestFixture : public ::testing::TestWithParam<char const *>
{
protected:
    std::locale locale;

    void SetUp() override { locale = std::locale(GetParam()); }
};

/** Global locale testing fixture.
 *
 * Parametrized test fixtures which automatically sets up and restores global locale.
 * \code{.cpp}
 * class FooTestFixture : public GlobalLocaleTestFixture {};
 * TEST_P(FooTestFixture, Test1)
 * {
 *    test_logic
 * }
 * TEST_P(FooTestFixture, Test2) { .. }
 * INSTANTIATE_TEST_SUITE_P(TestSuite,
 *                           FooTestFixture,
 *                           testing::Values("C", "de_DE.UTF8"));
 * \endcode
 */
class GlobalLocaleTestFixture : public LocaleTestFixture
{
    std::locale backup;

protected:
    void SetUp() override
    {
        try {
            LocaleTestFixture::SetUp();
        } catch (std::exception const &e) {
            GTEST_SKIP() << "Skipping locale '" << GetParam() << "' not available\n";
        }
        backup = std::locale::global(locale);
    }

    void TearDown() override
    {
        LocaleTestFixture::TearDown();
        std::locale::global(backup);
    }
};

} // namespace

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
