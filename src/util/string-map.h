// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Hashmap implementation which uses boost::unordered_map to provide templated overloads
 * on some methods. This will allow us to use Glib::ustring, char* and string_view
 * to search for keys without creating any unwanted copies.
 * Derived from the definition of unordered_map in bits/unordered_map.h found in the glibc
 * header files.
 * This is a temporary data structure to be used until the switch to C++20.
 *//*
 * Authors:
 *   Mohammad Aadil Shabier
 *
 * Copyright (C) 2022 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_UTIL_STRING_MAP_H
#define INKSCAPE_UTIL_STRING_MAP_H

#include <boost/unordered_map.hpp>
#include <glibmm/ustring.h>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

namespace Inkscape {
namespace Util {

#ifndef DOXYGEN_SHOULD_SKIP_THIS

namespace Impl {
struct Hash
{
    using hash_type = std::hash<std::string_view>;
    using is_transparent = void;

    auto operator()(std::string_view const &str) const { return hash_type{}(str); }
    auto operator()(std::string const &str) const { return hash_type{}(str); }
    auto operator()(Glib::ustring const &str) const { return hash_type{}(str.raw()); }
    auto operator()(char const *str) const { return hash_type{}(str); }
};

template <typename K>
struct Conv
{
    K const &k;
    operator std::string_view() { return k; }
};

template <>
struct Conv<Glib::ustring>
{
    Glib::ustring const &k;
    operator std::string_view() { return k.raw(); }
};

struct Comp
{
    using is_transparent = void;

    template <typename A, typename B>
    bool operator()(A const &a, B const &b) const { return std::equal_to<std::string_view>{}(Conv<A>{a}, Conv<B>{b}); }
};

} // namespace Impl
#endif // DOXYGEN_SHOULD_SKIP_THIS

template <typename Mapped,
          typename Alloc = std::allocator<std::pair<Glib::ustring const, Mapped>> >
class StringMap
{
public:
    using base_type            = boost::unordered_map<Glib::ustring, Mapped, Impl::Hash, Impl::Comp, Alloc>;
    using key_type             = typename base_type::key_type;
    using value_type           = typename base_type::value_type;
    using mapped_type          = typename base_type::mapped_type;
    using hasher               = typename base_type::hasher;
    using key_equal            = typename base_type::key_equal;
    using allocator_type       = typename base_type::allocator_type;
    using pointer              = typename allocator_type::pointer;
    using const_pointer        = typename allocator_type::const_pointer;
    using reference            = typename allocator_type::reference;
    using const_reference      = typename allocator_type::const_reference;
    using size_type            = typename base_type::size_type;
    using difference_type      = typename base_type::difference_type;
    using iterator             = typename base_type::iterator;
    using const_iterator       = typename base_type::const_iterator;
    using local_iterator       = typename base_type::local_iterator;
    using const_local_iterator = typename base_type::const_local_iterator;
    using node_type            = typename base_type::node_type;
    using insert_return_type   = typename base_type::insert_return_type;

    // construct/destroy/copy

    /// Default constructor.
    StringMap() = default;

    explicit StringMap(size_type n, allocator_type const &a = allocator_type())
        : _umap(n, hasher(), key_equal(), a)
    {}

    template <typename InputIterator>
    StringMap(InputIterator first, InputIterator last, size_type n = 0, allocator_type const &a = allocator_type())
        : _umap(first, last, n, hasher(), key_equal(), a)
    {}

    /// Copy constructor.
    StringMap(StringMap const &) = default;

    /// Move constructor.
    StringMap(StringMap &&) = default;

    explicit StringMap(allocator_type const &a)
        : _umap(a)
    {}

    StringMap(StringMap const &umap, allocator_type const &a)
        : _umap(umap._umap, a)
    {}

    StringMap(StringMap &&umap, allocator_type const &a)
        : _umap(std::move(umap._umap), a)
    {}

    StringMap(std::initializer_list<value_type> l, size_type n = 0, allocator_type const &a = allocator_type())
        : _umap(l, n, hasher(), key_equal(), a)
    {}

    /// Copy assignment operator.
    StringMap &operator=(StringMap const &) = default;

    /// Move assignment operator.
    StringMap &operator=(StringMap &&) = default;

    StringMap &operator=(std::initializer_list<value_type> l)
    {
        _umap = l;
        return *this;
    }

    ///  Returns the allocator object used by the %StringMap.
    allocator_type get_allocator() const noexcept { return _umap.get_allocator(); }

    // size and capacity:

    ///  Returns true if the %StringMap is empty.
    [[nodiscard]] bool empty() const noexcept { return _umap.empty(); }

    ///  Returns the size of the %StringMap.
    size_type size() const noexcept { return _umap.size(); }

    ///  Returns the maximum size of the %StringMap.
    size_type max_size() const noexcept { return _umap.max_size(); }

    // iterators.

    iterator begin() noexcept { return _umap.begin(); }

    //@{
    const_iterator begin() const noexcept { return _umap.begin(); }

    const_iterator cbegin() const noexcept { return _umap.begin(); }
    //@}

    iterator end() noexcept { return _umap.end(); }

    //@{
    const_iterator end() const noexcept { return _umap.end(); }

    const_iterator cend() const noexcept { return _umap.end(); }
    //@}

    // modifiers.

    template <typename... Args>
    std::pair<iterator, bool> emplace(Args &&...args)
    {
        return _umap.emplace(std::forward<Args>(args)...);
    }

    template <typename... Args>
    iterator emplace_hint(const_iterator pos, Args &&...args)
    {
        return _umap.emplace_hint(pos, std::forward<Args>(args)...);
    }

    /// Extract a node.
    node_type extract(const_iterator pos) { return _umap.extract(pos); }

    /// Extract a node.
    node_type extract(key_type const &key) { return _umap.extract(key); }

    /// Re-insert an extracted node.
    insert_return_type insert(node_type &&nh) { return _umap.insert(std::move(nh)); }

    /// Re-insert an extracted node.
    iterator insert(const_iterator, node_type &&nh) { return _umap.insert(std::move(nh)).position; }

    template <typename K, typename... Args>
    std::pair<iterator, bool> try_emplace(K const &k, Args &&...args)
    {
        iterator i = find(k);
        if (i == end()) {
            i = emplace(std::piecewise_construct, std::forward_as_tuple(k),
                        std::forward_as_tuple(std::forward<Args>(args)...))
                    .first;
            return {i, true};
        }
        return {i, false};
    }

    // move-capable overload
    template <typename K, typename... Args>
    std::pair<iterator, bool> try_emplace(K &&k, Args &&...args)
    {
        iterator i = find(k);
        if (i == end()) {
            i = emplace(std::piecewise_construct, std::forward_as_tuple(std::forward<K>(k)),
                        std::forward_as_tuple(std::forward<Args>(args)...))
                    .first;
            return {i, true};
        }
        return {i, false};
    }

    template <typename K, typename... Args>
    iterator try_emplace(const_iterator hint, K const &k, Args &&...args)
    {
        iterator i = find(k);
        if (i == end())
            i = emplace_hint(hint, std::piecewise_construct, std::forward_as_tuple(k),
                             std::forward_as_tuple(std::forward<Args>(args)...));
        return i;
    }

    // move-capable overload
    template <typename K, typename... Args>
    iterator try_emplace(const_iterator hint, K &&k, Args &&...args)
    {
        iterator i = find(k);
        if (i == end())
            i = emplace_hint(hint, std::piecewise_construct, std::forward_as_tuple(std::forward<K>(k)),
                             std::forward_as_tuple(std::forward<Args>(args)...));
        return i;
    }

    //@{
    std::pair<iterator, bool> insert(value_type const &x) { return _umap.insert(x); }

    std::pair<iterator, bool> insert(value_type &&x) { return _umap.insert(std::move(x)); }

    template <typename Pair>
    std::pair<iterator, bool> insert(Pair &&x)
    {
        return _umap.emplace(std::forward<Pair>(x));
    }
    //@}

    //@{
    iterator insert(const_iterator hint, value_type const &x) { return _umap.insert(hint, x); }

    iterator insert(const_iterator hint, value_type &&x) { return _umap.insert(hint, std::move(x)); }

    template <typename Pair>
    iterator insert(const_iterator hint, Pair &&x)
    {
        return _umap.emplace_hint(hint, std::forward<Pair>(x));
    }
    //@}

    template <typename InputIterator>
    void insert(InputIterator first, InputIterator last)
    {
        _umap.insert(first, last);
    }

    void insert(std::initializer_list<value_type> l) { _umap.insert(l); }

    template <typename K, typename Obj>
    std::pair<iterator, bool> insert_or_assign(K const &k, Obj &&obj)
    {
        iterator i = find(k);
        if (i == end()) {
            i = emplace(std::piecewise_construct, std::forward_as_tuple(k),
                        std::forward_as_tuple(std::forward<Obj>(obj)))
                    .first;
            return {i, true};
        }
        i->second = std::forward<Obj>(obj);
        return {i, false};
    }

    // move-capable overload
    template <typename K, typename Obj>
    std::pair<iterator, bool> insert_or_assign(K &&k, Obj &&obj)
    {
        iterator i = find(k);
        if (i == end()) {
            i = emplace(std::piecewise_construct, std::forward_as_tuple(std::forward<K>(k)),
                        std::forward_as_tuple(std::forward<Obj>(obj)))
                    .first;
            return {i, true};
        }
        i->second = std::forward<Obj>(obj);
        return {i, false};
    }

    template <typename K, typename Obj>
    iterator insert_or_assign(const_iterator hint, K const &k, Obj &&obj)
    {
        iterator i = find(k);
        if (i == end()) {
            return emplace_hint(hint, std::piecewise_construct, std::forward_as_tuple(k),
                                std::forward_as_tuple(std::forward<Obj>(obj)));
        }
        i->second = std::forward<Obj>(obj);
        return i;
    }

    // move-capable overload
    template <typename K, typename Obj>
    iterator insert_or_assign(const_iterator hint, K &&k, Obj &&obj)
    {
        iterator i = find(k);
        if (i == end()) {
            return emplace_hint(hint, std::piecewise_construct, std::forward_as_tuple(std::forward<K>(k)),
                                std::forward_as_tuple(std::forward<Obj>(obj)));
        }
        i->second = std::forward<Obj>(obj);
        return i;
    }

    //@{
    iterator erase(const_iterator position) { return _umap.erase(position); }

    iterator erase(iterator position) { return _umap.erase(position); }
    //@}

    size_type erase(key_type const &x) { return _umap.erase(x); }

    iterator erase(const_iterator first, const_iterator last) { return _umap.erase(first, last); }

    void clear() noexcept { _umap.clear(); }

    void swap(StringMap &x) noexcept(noexcept(StringMap::_umap.swap(x._umap))) { _umap.swap(x._umap); }

    void merge(StringMap &source) { _umap.merge(source); }

    void merge(StringMap &&source) { merge(source); }

    // observers.

    ///  Returns the hash functor object with which the %StringMap was
    ///  constructed.
    hasher hash_function() const { return _umap.hash_function(); }

    ///  Returns the key comparison object with which the %StringMap was
    ///  constructed.
    key_equal key_eq() const { return _umap.key_eq(); }

    // lookup.

    //@{
    template <typename K>
    iterator find(K const &x) { return _find(Impl::Conv<K>{x}); }

    template <typename K>
    const_iterator find(K const &x) const { return _find(Impl::Conv<K>{x}); }
    //@}

    size_type count(key_type const &x) const { return _umap.count(x); }

    bool contains(key_type const &x) const { return _umap.contains(x); }

    //@{
    std::pair<iterator, iterator> equal_range(key_type const &x) { return _umap.equal_range(x); }

    std::pair<const_iterator, const_iterator> equal_range(key_type const &x) const { return _umap.equal_range(x); }
    //@}

    //@{
    mapped_type &operator[](key_type const &k) { return _umap[k]; }

    mapped_type &operator[](key_type &&k) { return _umap[std::move(k)]; }
    //@}

    //@{
    mapped_type &at(key_type const &k) { return _umap.at(k); }

    mapped_type const &at(key_type const &k) const { return _umap.at(k); }
    //@}

    // bucket interface.

    /// Returns the number of buckets of the %StringMap.
    size_type bucket_count() const noexcept { return _umap.bucket_count(); }

    /// Returns the maximum number of buckets of the %StringMap.
    size_type max_bucket_count() const noexcept { return _umap.max_bucket_count(); }

    size_type bucket_size(size_type n) const { return _umap.bucket_size(n); }

    size_type bucket(key_type const &key) const { return _umap.bucket(key); }

    local_iterator begin(size_type n) { return _umap.begin(n); }

    //@{
    const_local_iterator begin(size_type n) const { return _umap.begin(n); }

    const_local_iterator cbegin(size_type n) const { return _umap.cbegin(n); }
    //@}

    local_iterator end(size_type n) { return _umap.end(n); }

    //@{
    const_local_iterator end(size_type n) const { return _umap.end(n); }

    const_local_iterator cend(size_type n) const { return _umap.cend(n); }
    //@}

    // hash policy.

    /// Returns the average number of elements per bucket.
    float load_factor() const noexcept { return _umap.load_factor(); }

    /// Returns a positive number that the %StringMap tries to keep the
    /// load factor less than or equal to.
    float max_load_factor() const noexcept { return _umap.max_load_factor(); }

    void max_load_factor(float z) { _umap.max_load_factor(z); }

    void rehash(size_type n) { _umap.rehash(n); }

    void reserve(size_type n) { _umap.reserve(n); }

    template <typename Tp, typename AllocTp>
    friend bool operator==(StringMap<Tp, AllocTp> const &, StringMap<Tp, AllocTp> const &);

private:
    iterator _find(std::string_view const &str) { return _umap.find(str, Impl::Hash{}, Impl::Comp{}); }

    const_iterator _find(std::string_view const &str) const { return _umap.find(str, Impl::Hash{}, Impl::Comp{}); }

private:
    base_type _umap;
};

template <class Tp, typename Alloc>
inline void swap(StringMap<Tp, Alloc> &x, StringMap<Tp, Alloc> &y) noexcept(noexcept(x.swap(y)))
{
    x.swap(y);
}

template <class Tp, typename Alloc>
inline bool operator==(StringMap<Tp, Alloc> const &x, StringMap<Tp, Alloc> const &y)
{
    return x._umap == y._umap;
}

template <class Tp, typename Alloc>
inline bool operator!=(StringMap<Tp, Alloc> const &x, StringMap<Tp, Alloc> const &y)
{
    return !(x == y);
}

} // namespace Util
} // namespace Inkscape

#endif // INKSCAPE_UTIL_STRING_MAP_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace .0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
