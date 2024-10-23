// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_SP_OBJECT_ITERATOR_H
#define SEEN_SP_OBJECT_ITERATOR_H

// #include <ranges>
#include "sp-object.h"

// This is depth-first SPObject tree traversal iterator.
// It starts visiting nodes from the bottom up. One consequence of this approach
// is that starting node will be visited last.
// This iterator can be used in a range for loop as well as std algorithms.

struct object_iterator {
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = SPObject*;
    using pointer           = SPObject*;
    using reference         = SPObject*;

    object_iterator(SPObject* start) : _p(start) {
        if (!_p) return;

        // descend
        _p = find_next(_p);
    }

    object_iterator(): _p(nullptr) {}
    object_iterator(const object_iterator& src) = default;
    object_iterator& operator = (const object_iterator& src) = default;

    // create an iterator that points past the object 'obj'
    static object_iterator get_end(SPObject* obj) {
        object_iterator end;
        if (obj) {
            end._p = end.find_next(obj->getNext());
        }
        return end;
    }

    bool operator == (const object_iterator& it) const { return it._p == _p; }
    bool operator != (const object_iterator& it) const { return !(it == *this); }

    value_type operator * () const { return _p; }
    value_type operator -> () const { return _p; }

    object_iterator& operator ++ () {
        _p = find_next(_p->getNext());
        return *this;
    }

    object_iterator operator ++ (int) {
        auto temp = *this;
        ++(*this);
        return temp;
    }

private:
    // find next candidate node to visit by trying to descend;
    // note that we can return the same node as input if it's a leaf
    SPObject* find_next(SPObject* next) {
        if (next) {
            // there's a next node; descend as low as possible
            while (auto child = next->firstChild()) {
                next = child;
            }
            return next;
        }
        else {
            // reached the last node at current level, go back up
            return _p ? _p->parent : nullptr;
        }
    }

    SPObject* _p;
};

// Expose begin and end functions for range for loop; such loop would visit all
// subnodes and leaves of input object including starting object itself.

inline object_iterator begin(SPObject* ob) {
    return object_iterator(ob);
}

inline  object_iterator end(SPObject* ob) {
    return object_iterator::get_end(ob);
}

// Object child elements as a range of iterators

// inline std::ranges::subrange<object_iterator> get_object_children(SPObject* ob) {
//     auto f = ob ? ob->firstChild() : nullptr;
//     if (!f) return std::ranges::subrange{object_iterator(nullptr), object_iterator(nullptr)};
//
//     return std::ranges::subrange{object_iterator(f), ++object_iterator(ob->lastChild())};
// }

#endif // SEEN_SP_OBJECT_ITERATOR_H
