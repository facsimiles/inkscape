// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author: Liam White
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_DISPLAY_DISPATCH_POOL_H
#define INKSCAPE_DISPLAY_DISPATCH_POOL_H

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Inkscape {

class dispatch_pool
{
public:
    using global_id = int;
    using local_id = int;
    using dispatch_func = std::function<void(global_id, local_id)>;

    explicit dispatch_pool(int size);
    ~dispatch_pool();

    void dispatch(int size, dispatch_func function);

    void dispatch_threshold(int size, bool threshold, dispatch_func function)
    {
        if (threshold) {
            dispatch(size, std::move(function));
        } else {
            for (auto i = global_id{}; i < global_id{size}; i++) {
                function(i, local_id{});
            }
        }
    }

    int size() const { return _threads.size(); }

private:
    void thread_func(local_id id);

private:
    std::mutex _dispatch_lock;
    std::mutex _lock;
    std::condition_variable _cv;
    std::vector<std::thread> _threads;
    dispatch_func _function;
    global_id _available_work;
    global_id _completed_work;
    global_id _target_work;
    bool _shutdown;
};

std::shared_ptr<dispatch_pool> get_global_dispatch_pool();

} // namespace Inkscape

#endif // INKSCAPE_DISPLAY_DISPATCH_POOL_H

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
