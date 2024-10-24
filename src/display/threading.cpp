// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author: Liam White
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <atomic>

#include "dispatch-pool.h"
#include "threading.h"

namespace Inkscape {

namespace {

std::atomic<int> g_num_filter_threads = 4;
std::mutex g_dispatch_lock;
std::shared_ptr<dispatch_pool> g_dispatch_pool;
int g_dispatch_threads;

} // namespace

void set_num_filter_threads(int num_filter_threads)
{
    g_num_filter_threads.store(num_filter_threads, std::memory_order_relaxed);
}

std::shared_ptr<dispatch_pool> get_global_dispatch_pool()
{
    int const num_threads = g_num_filter_threads.load(std::memory_order_relaxed);

    std::scoped_lock lk(g_dispatch_lock);

    if (g_dispatch_pool && num_threads == g_dispatch_threads) {
        return g_dispatch_pool;
    }

    g_dispatch_pool = std::make_shared<dispatch_pool>(num_threads);
    g_dispatch_threads = num_threads;

    return g_dispatch_pool;
}

}

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
