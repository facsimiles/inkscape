// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author: Liam White
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "dispatch-pool.h"

namespace Inkscape {

dispatch_pool::dispatch_pool(int size)
    : _available_work()
    , _completed_work()
    , _target_work()
    , _shutdown()
{
    _threads.reserve(size);

    for (int i = 0; i < size; ++i) {
        _threads.emplace_back([i, this] { thread_func(local_id{i}); });
    }
}

dispatch_pool::~dispatch_pool()
{
    // TODO C++20: this would be completely trivial with jthread
    {
        std::scoped_lock lk(_lock);
        _shutdown = true;
        _cv.notify_all();
    }

    for (auto &thread : _threads) {
        thread.join();
    }

    _threads.clear();
}

void dispatch_pool::dispatch(int size, dispatch_func function)
{
    std::scoped_lock lk(_dispatch_lock);
    std::unique_lock lk2(_lock);

    _available_work = global_id{};
    _completed_work = global_id{};
    _target_work = global_id{size};
    _function = std::move(function);

    _cv.notify_all();
    _cv.wait(lk2, [&] { return _completed_work == _target_work; });

    // Release any extra memory held by the function
    _function = {};
}

void dispatch_pool::thread_func(local_id id)
{
    std::unique_lock lk(_lock);

    while (true) {
        _cv.wait(lk, [&] { return _shutdown || _available_work < _target_work; });

        if (_shutdown) {
            // When shutdown is requested, stop immediately
            return;
        }

        // Take some available work
        global_id index = _available_work++;

        // Execute the function
        {
            lk.unlock();
            _function(index, id);
            lk.lock();
        }

        // Signal completion
        _completed_work++;

        if (_completed_work == _target_work) {
            _cv.notify_all();
        }
    }
}

} // namespace Inkscape

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
