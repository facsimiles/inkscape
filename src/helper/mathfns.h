// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Mathematical/numerical functions.
 *
 * Authors:
 *   Johan Engelen <goejendaagh@zonnet.nl>
 *
 * Copyright (C) 2007 Johan Engelen
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_HELPER_MATHFNS_H
#define INKSCAPE_HELPER_MATHFNS_H

#include <bit>
#include <cmath>
#include <concepts>

namespace Inkscape::Util {

/**
 * \return x rounded to the nearest multiple of c1 plus c0.
 *
 * \note
 * If c1==0 (and c0 is finite), then returns +/-inf.  This makes grid spacing of zero
 * mean "ignore the grid in this dimension".
 */
inline double round_to_nearest_multiple_plus(double x, double c1, double c0)
{
    return std::floor((x - c0) / c1 + 0.5) * c1 + c0;
}

/**
 * \return x rounded to the lower multiple of c1 plus c0.
 *
 * \note
 * If c1 == 0 (and c0 is finite), then returns +/-inf. This makes grid spacing of zero
 * mean "ignore the grid in this dimension".
 */
inline double round_to_lower_multiple_plus(double x, double c1, double c0 = 0.0)
{
    return std::floor((x - c0) / c1) * c1 + c0;
}

/**
 * \return x rounded to the upper multiple of c1 plus c0.
 *
 * \note
 * If c1 == 0 (and c0 is finite), then returns +/-inf. This makes grid spacing of zero
 * mean "ignore the grid in this dimension".
 */
inline double round_to_upper_multiple_plus(double x, double const c1, double const c0 = 0)
{
    return std::ceil((x - c0) / c1) * c1 + c0;
}

/// Returns floor(log_2(x)), assuming x >= 1; if x == 0, returns -1.
int constexpr floorlog2(std::unsigned_integral auto x)
{
    return std::bit_width(x) - 1;
}

template <std::unsigned_integral T, T size>
int constexpr index_to_binary_bucket(T index)
{
    return floorlog2((index - 1) / size) + 1;
}

/// Returns \a a mod \a b, always in the range 0..b-1, assuming b >= 1.
template <std::integral T>
T constexpr safemod(T a, T b)
{
    a %= b;
    return a < 0 ? a + b : a;
}

/// Returns \a a rounded down to the nearest multiple of \a b, assuming b >= 1.
template <std::integral T>
T constexpr round_down(T a, T b)
{
    return a - safemod(a, b);
}

/// Returns \a a rounded up to the nearest multiple of \a b, assuming b >= 1.
template <std::integral T>
T constexpr round_up(T a, T b)
{
    return round_down(a - 1, b) + b;
}

template <typename T>
concept strictly_ordered = requires(T a, T b) {
    { a < b } -> std::convertible_to<bool>;
    { a > b } -> std::convertible_to<bool>;
};

/**
 * Just like std::clamp, except it doesn't deliberately crash if lo > hi due to rounding errors,
 * so is safe to use with floating-point types. (Note: compiles to branchless.)
 */
template <strictly_ordered T>
T safeclamp(T val, T lo, T hi)
{
    if (val < lo) {
        return lo;
    }
    if (val > hi) {
        return hi;
    }
    return val;
}
} // namespace Inkscape::Util

#endif // INKSCAPE_HELPER_MATHFNS_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
