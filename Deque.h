#include <iostream>
#include <bits/stl_construct.h>
#include <bits/allocator.h>
#include <bits/stl_uninitialized.h>
#include <type_traits>
#include <bits/stl_algobase.h>
#include <iterator>
#include <assert.h>
#include <malloc.h>
using namespace std;

#ifndef __DEQUE_H__
#define __DEQUE_H__

constexpr size_t __deque_buf_size__(size_t __n)
{
    return __n > 512 ? 1 : size_t(512) / __n;
}

template <class Tp, class Ptr, class Ref>
struct __DequeIterator
{
    typedef Tp value_type;
    typedef Ptr pointer;
    typedef Ref reference;
    typedef std::random_access_iterator_tag iterator_category;
    typedef ptrdiff_t difference_type;
    typedef size_t size_type;
    typedef __DequeIterator<Tp, Tp *, Tp &> iterator;
    typedef __DequeIterator<Tp, Ptr, Ref> Self;
    typedef Tp **map_ptr;

    map_ptr m_node;
    pointer m_cur;

    __DequeIterator(map_ptr node = 0, pointer cur = 0)
        : m_node(node), m_cur(cur) {}

    __DequeIterator(const iterator &it)
        : m_node(it.m_node), m_cur(it.m_cur) {}

    pointer base() const
    {
        return m_cur;
    }

    static size_type _M_BufSize()
    {
        return __deque_buf_size__(sizeof(Tp));
    }

    Self &operator++()
    {
        m_cur++;
        if (m_cur - *m_node == _M_BufSize())
        {
            ++m_node;
            m_cur = *m_node;
        }
        return *this;
    }

    Self operator++(int)
    {
        Self tmp = *this;
        ++*this;
        return tmp;
    }

    Self &operator--()
    {
        if (m_cur == *m_node)
        {
            --m_node;
            m_cur = *m_node + _M_BufSize();
        }
        --m_cur;
        return *this;
    }

    Self operator--(int)
    {
        Self tmp = *this;
        --*this;
        return tmp;
    }

    Self &operator+=(difference_type offset)
    {
        size_type bufSize = _M_BufSize();
        difference_type checkOffset = m_cur - *m_node + offset;
        if (checkOffset >= 0 && checkOffset < bufSize)
        {
            m_cur += offset;
        }
        else
        {
            difference_type nodeOffset
                = checkOffset > 0
                  ? checkOffset / difference_type(bufSize)
                  : -(-checkOffset - 1) / difference_type(bufSize) - 1;
            m_node += nodeOffset;
            m_cur = *m_node + (checkOffset - nodeOffset * difference_type(bufSize));
        }
        return *this;
    }

    Self operator+(difference_type offset)
    {
        Self tmp = *this;
        tmp += offset;
        return tmp;
    }

    Self &operator-=(difference_type offset)
    {
        *this += (-offset);
        return *this;
    }

    Self operator-(difference_type offset)
    {
        Self tmp = *this;
        tmp -= offset;
        return tmp;
    }

    difference_type operator-(const Self &it) const
    {
        size_type bufSize = _M_BufSize();
        return (bufSize - (it.m_cur - *it.m_node) + (m_cur - *m_node))
               + (m_node - it.m_node - 1) * difference_type(bufSize);
    }

    reference operator*() const
    {
        return *m_cur;
    }

    pointer operator->() const
    {
        return m_cur;
    }

    bool operator==(const Self &it) const
    {
        return m_node == it.m_node && m_cur == it.m_cur;
    }

    bool operator!=(const Self &it) const
    {
        return !(operator==(it));
    }

    bool operator>(const Self &it) const
    {
        return m_node != it.m_node
               ? m_node > it.m_node : m_cur > it.m_cur;
    }

    bool operator<(const Self &it) const
    {
        return m_node != it.m_node
               ? m_node < it.m_node : m_cur < it.m_cur;
    }

    bool operator>=(const Self &it) const
    {
        return !(operator<(it));
    }

    bool operator<=(const Self &it) const
    {
        return !(operator>(it));
    }
};

template <class Tp>
class Deque
{
public:
    typedef Tp value_type;
    typedef Tp *pointer;
    typedef Tp &reference;
    typedef Deque<Tp> Self;
    typedef const Tp *const_pointer;
    typedef const Tp &const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef __DequeIterator<Tp, Tp *, Tp &> iterator;
    typedef __DequeIterator<Tp, const Tp *, const Tp &> const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
    typedef Tp **map_ptr;

protected:
    map_ptr m_map;
    iterator m_start, m_finish;
    size_type m_mapSize;
    // for compatible with old c++ uninitialized operator;
    // it is not in use, in Deque, use malloc and free;
    allocator<Tp> m_allocator;
    const static size_type __buf_size = __deque_buf_size__(sizeof(Tp));

private:

    template <class Type>
    Type *__alloc(size_type __n)
    {
        Type *__r = (Type *)malloc(__n * sizeof(Type));
        if (!__r)
            throw std::bad_alloc();
        return __r;
    }

    template <class Pointer>
    void __free(Pointer &__ptr)
    {
        free(__ptr);
        __ptr = nullptr;
    }

    map_ptr __alloc_map(size_type __n)
    {
        return __alloc<pointer>(__n);
    }

    void __dealloc_map()
    {
        __free<map_ptr>(m_map);
        m_mapSize = 0;
    }

    pointer __alloc_buffer()
    {
        return __alloc<Tp>(this->__buf_size);
    }

    void __dealloc_buffer(map_ptr __node)
    {
        __free<pointer>(*__node);
    }

    void __create_buffer(map_ptr __first, map_ptr __last)
    {
        map_ptr __alloc_ptr;
        try
        {
            for (__alloc_ptr = __first;
                    __alloc_ptr < __last;
                    ++__alloc_ptr)
                *__alloc_ptr = __alloc_buffer();
        }
        catch (const std::bad_alloc &e)
        {
            for (--__alloc_ptr;
                    __alloc_ptr >= __first;
                    --__alloc_ptr)
                __dealloc_buffer(__alloc_ptr);
            throw e;
        }
    }

    void __dealloc_buffer(map_ptr __first, map_ptr __last)
    {
        for (map_ptr __alloc_ptr = __first;
                __alloc_ptr < __last;
                ++__alloc_ptr)
            __free(*__alloc_ptr);
    }

private:

    void __destroy(iterator __first, iterator __last)
    {
        size_type __buf_size = this->__buf_size;
        for (map_ptr __dealloc_ptr = __first.m_node + 1;
                __dealloc_ptr < __last.m_node;
                __dealloc_ptr++)
            _Destroy(*__dealloc_ptr,
                     *__dealloc_ptr + __buf_size);
        if (__first.m_node != __last.m_node)
        {
            _Destroy(__first.m_cur, *(__first.m_node) + __buf_size);
            _Destroy(*(__last.m_node), __last.m_cur);
        }
    }

    iterator __fill_n(iterator __result, size_type __n, const value_type &__val)
    {
        size_type __len, __clen;
        pointer __p;
        size_type __buf_size = this->__buf_size;
        __len = __n;
        while (__len > 0)
        {
            __p = __result.base();
            __clen = __buf_size - size_type(__result.m_cur - *(__result.m_node));
            __clen = min(__len, __clen);
            std::fill_n(__p, __clen, __val);
            __len -= __clen;
            __result += __clen;
        }
        return __result;
    }

    iterator __fill(iterator __first, iterator __last, const value_type& __val)
    {
        return this->__fill_n(__first, __last - __first, __val);
    }

    pointer __n_copy_dispatch(pointer __first, size_type __clen, pointer __p, __true_type)
    {
        using __assignable = is_copy_assignable<Tp>;
        static_assert(__assignable::value, "type is not assignable");
        __builtin_memmove(__p, __first, sizeof(Tp) * __clen);
        return __first + __clen;
    }

    template <typename Iterable>
    Iterable __n_copy_dispatch(Iterable __first, size_type __clen, pointer __p, __false_type)
    {
        pointer __q = __p + __clen;
        while (__p != __q)
        {
            *__p = *__first;
            ++__p, ++__first;
        }
        return __first;
    }

    pointer __range_copy_dispatch(pointer __first, pointer __last,
                                  pointer __p, size_type,
                                  size_type *pSize, __true_type)
    {
        using __assignable = is_copy_assignable<Tp>;
        static_assert(__assignable::value, "type is not assignable");
        // size_type __clen = min(__last - __first, __len);
        __builtin_memmove(__p, __first, (__last - __first) * sizeof(Tp));
        if (pSize)
            *pSize = __last - __first;
        return __last;
    }

    template <typename Iterable>
    Iterable __range_copy_dispatch(Iterable __first, Iterable __last,
                                   pointer __p, size_type __len,
                                   size_type *pSize, __false_type)
    {
        size_type __res = __len;
        while (__res > 0 && __first != __last)
        {
            *__p = *__first;
            ++__p, ++__first;
            --__res;
        }
        if (pSize)
            *pSize = __len - __res;
        return __first;
    }

    pointer __n_move_dispatch(pointer __first, size_type __clen, pointer __p, __true_type)
    {
        using __assignable = is_move_assignable<Tp>;
        static_assert(__assignable::value, "type is not assignable" );
        __builtin_memmove(__p, __first, sizeof(Tp) * __clen);
        return __first + __clen;
    }

    template <typename Iterable>
    Iterable __n_move_dispatch(Iterable __first, size_type __clen, pointer __p, __false_type)
    {
        pointer __q = __p + __clen;
        while (__p != __q)
        {
            *__p = std::move(*__first);
            ++__p, ++__first;
        }
        return __first;
    }

    pointer __range_move_dispatch(pointer __first, pointer __last,
                                  pointer __p, size_type,
                                  size_type *pSize, __true_type)
    {
        using __assignable = is_move_assignable<Tp>;
        static_assert(__assignable::value, "type is not assignable");
        // size_type __clen = min(__last - __first, __len);
        __builtin_memmove(__p, __first, (__last - __first) * sizeof(Tp));
        if (pSize)
            *pSize = __last - __first;
        return __last;
    }

    template <typename Iterable>
    Iterable __range_move_dispatch(Iterable __first, Iterable __last,
                                   pointer __p, size_type __len,
                                   size_type *pSize, __false_type)
    {
        size_type __res = __len;
        while (__res > 0 && __first != __last)
        {
            *__p = std::move(*__first);
            // *__p = *__first;
            ++__p, ++__first;
            --__res;
        }
        if (pSize)
            *pSize = __len - __res;
        return __first;
    }

    iterator __copy(const_iterator __first, const_iterator __last, iterator __result)
    {
        size_type __len = __last - __first;
        size_type __clen;
        const_pointer __p;
        size_type __buf_size = this->__buf_size;
        while (__len > 0)
        {
            __p = __first.base();
            __clen = min(__buf_size - (size_type)(__first.m_cur - *(__first.m_node)),
                         __buf_size - (size_type)(__result.m_cur - *(__result.m_node)));
            __clen = min(__clen, __len);
            std::copy(__p, __p + __clen, __result.base());
            __first += __clen;
            __result += __clen;
            __len -= __clen;
        }
        return __result;
    }

    iterator __copy2(iterator __first, iterator __last, iterator __result)
    {
        return this->__copy(__first, __last, __result);
    }

    iterator __copy2(const_iterator __first, const_iterator __last, iterator __result)
    {
        return this->__copy(__first, __last, __result);
    }

    template <typename Iterable>
    iterator __n_copy2(Iterable __first, size_type __n, iterator __result, Iterable* __pIt)
    {
        size_type __len = __n;
        if (__len == 0)
        {
            if (__pIt)
                *__pIt = __first;
            return __result;
        }
        typedef typename iterator_traits<Iterable>::value_type
        __iterable_valuetype;
        using __type = typename conditional<
                       __is_trivial(__iterable_valuetype)
                       && __are_same<__iterable_valuetype, value_type>::__value,
                       typename __is_pointer<Iterable>::__type,
                       __false_type>::type;
        size_type __clen;
        size_type __buf_size = this->__buf_size;
        pointer __p;
        while (__len > 0)
        {
            __p = __result.base();
            __clen = __buf_size - size_type(__result.m_cur - *(__result.m_node));
            __clen = min(__len, __clen);
            __first = this->__n_copy_dispatch(
                          __first, __clen, __p, __type());
            __len -= __clen;
            __result += __clen;
        }
        if (__pIt)
            *__pIt = __first;
        return __result;
    }

    template <typename Iterable>
    iterator __range_copy2(Iterable __first, Iterable __last, iterator __result)
    {
        if (__first == __last) return __result;
        typedef typename iterator_traits<Iterable>::value_type
        __iterable_valuetype;
        using __type = typename conditional<
                       __is_trivial(__iterable_valuetype)
                       && __are_same<__iterable_valuetype, value_type>::__value,
                       typename __is_pointer<Iterable>::__type,
                       __false_type>::type;
        size_type __clen, __len;
        size_type __buf_size = this->__buf_size;
        pointer __p;
        while (__first != __last)
        {
            __p = __result.base();
            __len = __buf_size - size_type(__result.m_cur - *(__result.m_node));
            __first = this->__range_copy_dispatch(
                          __first, __last, __p,
                          __len, &__clen, __type());
            // __len -= __clen;
            __result += __clen;
        }
        return __result;
    }

    template <typename Iterable>
    iterator __copy2(Iterable __first, Iterable __last, iterator __result)
    {
        return this->__range_copy2<Iterable>(__first, __last, __result);
    }

    iterator __copy_backward(const_iterator __first, const_iterator __last, iterator __result)
    {
        size_type __len = __last - __first;
        size_type __clen;
        const_pointer __p;
        --__last, --__result;
        while (__len > 0)
        {
            __p = __last.base();
            __clen = min(size_type(__last.m_cur - *(__last.m_node) + 1),
                         size_type(__result.m_cur - *(__result.m_node) + 1));
            __clen = min(__clen, __len);
            ++__p;
            std::copy_backward(__p - __clen, __p,
                               (__result.base() + 1));
            __last -= __clen;
            __result -= __clen;
            __len -= __clen;
        }
        return __result;
    }

    iterator __move(const_iterator __first, const_iterator __last, iterator __result)
    {
        size_type __len = __last - __first;
        size_type __clen;
        const_pointer __p;
        size_type __buf_size = this->__buf_size;
        while (__len > 0)
        {
            __p = __first.base();
            __clen = min(__buf_size - size_type(__first.m_cur - *(__first.m_node)),
                         __buf_size - size_type(__result.m_cur - *(__result.m_node)));
            __clen = min(__clen, __len);
            std::move(__p, __p + __clen, __result.base());
            __first += __clen;
            __result += __clen;
            __len -= __clen;
        }
        return __result;
    }

    iterator __move2(iterator __first, iterator __last, iterator __result)
    {
        return this->__move(__first, __last, __result);
    }

    iterator __move2(const_iterator __first, const_iterator __last, iterator __result)
    {
        return this->__move(__first, __last, __result);
    }

    template <typename Iterable>
    iterator __n_move2(Iterable __first, size_type __n, iterator __result, Iterable* __pIt)
    {
        size_type __len = __n;
        if (__len == 0)
        {
            if (__pIt)
                *__pIt = __first;
            return __result;
        }
        typedef typename iterator_traits<Iterable>::value_type
        __iterable_valuetype;
        using __type = typename conditional<
                       __is_trivial(__iterable_valuetype)
                       && __are_same<__iterable_valuetype, value_type>::__value,
                       typename __is_pointer<Iterable>::__type,
                       __false_type>::type;
        size_type __clen;
        pointer __p;
        size_type __buf_size = this->__buf_size;
        while (__len > 0)
        {
            __p = __result.base();
            __clen = __buf_size - size_type(__result.m_cur - *(__result.m_node));
            __clen = min(__len, __clen);
            __first = this->__n_move_dispatch(
                          __first, __clen, __p, __type());
            __result += __clen;
            __len -= __clen;
        }
        if (__pIt)
            *__pIt = __first;
        return __result;
    }

    template <typename Iterable>
    iterator __range_move2(Iterable __first, Iterable __last, iterator __result)
    {
        if (__first == __last) return __result;
        typedef typename iterator_traits<Iterable>::value_type
        __iterable_valuetype;
        using __type = typename conditional<
                       __is_trivial(__iterable_valuetype)
                       && __are_same<__iterable_valuetype, value_type>::__value,
                       typename __is_pointer<Iterable>::__type,
                       __false_type>::type;
        size_type __clen, __len;
        size_type __buf_size = this->__buf_size;
        pointer __p;
        while (__first != __last)
        {
            __p = __result.base();
            __len = __buf_size - size_type(__result.m_cur - *(__result.m_node));
            __first = this->__range_move_dispatch(
                          __first, __last, __p,
                          __len, &__clen, __type());
            // __len -= __clen;
            __result += __clen;
        }
        return __result;
    }

    template <typename Iterable>
    iterator __move2(Iterable __first, Iterable __last, iterator __result)
    {
        return this->__range_move2<Iterable>(__first, __last, __result);
    }

    iterator __move_backward(const_iterator __first, const_iterator __last, iterator __result)
    {
        size_type __len = __last - __first;
        size_type __clen;
        const_pointer __p;
        --__last, --__result;
        while (__len > 0)
        {
            __p = __last.base();
            __clen = min(__last.m_cur - *(__last.m_node) + 1,
                         __result.m_cur - *(__result.m_node) + 1);
            __clen = min(__clen, __len);
            ++__p;
            std::move_backward(__p - __clen, __p,
                               (__result.base() + 1));
            __last -= __clen;
            __result -= __clen;
            __len -= __clen;
        }
        return __result;
    }

    iterator __uninitialized_copy(const_iterator __first, const_iterator __last, iterator __result)
    {
        size_type __len = __last - __first;
        size_type __clen;
        size_type __buf_size = this->__buf_size;
        const_pointer __p;
        while (__len > 0)
        {
            __p = __first.base();
            __clen = min(__buf_size - size_type(__first.m_cur - *(__first.m_node)),
                         __buf_size - size_type(__result.m_cur - *(__result.m_node)));
            __clen = min(__clen, __len);
            std::uninitialized_copy(__p, __p + __clen, __result.base());
            __first += __clen;
            __result += __clen;
            __len -= __clen;
        }
        return __result;
    }

    iterator __uninitialized_copy2(iterator __first, iterator __last, iterator __result)
    {
        return this->__uninitialized_copy(__first, __last, __result);
    }

    iterator __uninitialized_copy2(const_iterator __first, const_iterator __last, iterator __result)
    {
        return this->__uninitialized_copy(__first, __last, __result);
    }

    template <typename Iterable>
    iterator __uninitialized_n_copy2(Iterable __first, size_type __n, iterator __result, Iterable* __pIt)
    {
        size_type __len = __n;
        if (__len == 0)
        {
            if (__pIt)
                *__pIt = __first;
            return __result;
        }
        typedef typename iterator_traits<Iterable>::value_type
        __iterable_valuetype;
        const bool __trivial = __is_trivial(__iterable_valuetype)
                               && __is_trivial(value_type);
        size_type __clen;
        size_type __buf_size = this->__buf_size;
        pointer __p;
        if (!__trivial)
        {
            while (__len > 0)
            {
                __p = __result.base();
                __clen = __buf_size - size_type(__result.m_cur - *(__result.m_node));
                __clen = min(__len, __clen);
                for (size_type __i = 0; __i != __clen; ++__i)
                {
                    _Construct(__p, *__first);
                    ++__p, ++__first;
                }
                __len -= __clen;
                __result += __clen;
            }
        }
        else
        {
            using __type = typename conditional<
                           __are_same<__iterable_valuetype, value_type>::__value,
                           typename __is_pointer<Iterable>::__type,
                           __false_type>::type;
            while (__len > 0)
            {
                __p = __result.base();
                __clen = __buf_size - size_type(__result.m_cur - *(__result.m_node));
                __clen = min(__len, __clen);
                __first = this->__n_copy_dispatch(
                              __first, __clen, __p, __type());
                __len -= __clen;
                __result += __clen;
            }
        }
        if (__pIt)
            *__pIt = __first;
        return __result;
    }

    template <typename Iterable>
    iterator __uninitialized_range_copy2(Iterable __first, Iterable __last, iterator __result)
    {
        // difference_type __len = __n;
        if (__first == __last) return __result;
        typedef typename iterator_traits<Iterable>::value_type
        __iterable_valuetype;
        const bool __trivial = __is_trivial(__iterable_valuetype)
                               && __is_trivial(value_type);
        size_type __clen, __len;
        size_type __buf_size = this->__buf_size;
        pointer __p;
        if (!__trivial)
        {
            while (__first != __last)
            {
                __p = __result.base();
                __len = __buf_size - size_type(__result.m_cur - *(__result.m_node));
                for (__clen = 0; __clen != __len && __first != __last; ++__clen)
                {
                    _Construct(__p, *__first);
                    ++__p, ++__first;
                }
                // __len -= __clen;
                __result += __clen;
            }
        }
        else
        {
            using __type = typename conditional<
                           __are_same<__iterable_valuetype, value_type>::__value,
                           typename __is_pointer<Iterable>::__type,
                           __false_type>::type;
            while (__first != __last)
            {
                __p = __result.base();
                __len = (__buf_size) - size_type(__result.m_cur - *(__result.m_node));
                __first = this->__range_copy_dispatch(
                              __first, __last, __p,
                              __len, &__clen, __type());
                // __len -= __clen;
                __result += __clen;
            }
        }
        return __result;
    }

    template <typename Iterable>
    iterator __uninitialized_copy2(Iterable __first, Iterable __last, iterator __result)
    {
        return this->__uninitialized_range_copy2<Iterable>(__first, __last, __result);
    }

    iterator __uninitialized_move(const_iterator __first, const_iterator __last, iterator __result)
    {
        size_type __len = __last - __first;
        size_type __clen;
        size_type __buf_size = this->__buf_size;
        const_pointer __p;
        while (__len > 0)
        {
            __p = __first.base();
            __clen = min(__buf_size - size_type(__first.m_cur - *(__first.m_node)),
                         __buf_size - size_type(__result.m_cur - *(__result.m_node)));
            __clen = min(__clen, __len);
            std::__uninitialized_move_a(
                __p, __p + __clen, __result.base(), m_allocator);
            __first += __clen;
            __result += __clen;
            __len -= __clen;
        }
        return __result;
    }

    iterator __uninitialized_move2(iterator __first, iterator __last, iterator __result)
    {
        return this->__uninitialized_move(__first, __last, __result);
    }

    iterator __uninitialized_move2(const_iterator __first, const_iterator __last, iterator __result)
    {
        return this->__uninitialized_move(__first, __last, __result);
    }

    template <typename Iterable>
    iterator __uninitialized_n_move2(Iterable __first, size_type __n, iterator __result, Iterable* __pIt)
    {
        size_type __len = __n;
        if (__len == 0)
        {
            if (__pIt)
                *__pIt = __first;
            return __result;
        }
        typedef typename iterator_traits<Iterable>::value_type
        __iterable_valuetype;
        const bool __trivial = __is_trivial(__iterable_valuetype)
                               && __is_trivial(value_type);
        size_type __clen;
        size_type __buf_size = this->__buf_size;
        pointer __p;
        if (!__trivial)
        {
            while (__len > 0)
            {
                __p = __result.base();
                __clen = __buf_size - size_type(__result.m_cur - *(__result.m_node));
                __clen = min(__len, __clen);
                for (size_type __i = 0; __i != __clen; ++__i)
                {
                    _Construct(__p, std::move(*__first));
                    ++__p, ++__first;
                }
                __len -= __clen;
                __result += __clen;
            }
        }
        else
        {
            using __type = typename conditional<
                           __are_same<__iterable_valuetype, value_type>::__value,
                           typename __is_pointer<Iterable>::__type,
                           __false_type>::type;
            while (__len > 0)
            {
                __p = __result.base();
                __clen = __buf_size - size_type(__result.m_cur - *(__result.m_node));
                __clen = min(__len, __clen);
                __first = this->__n_move_dispatch(
                              __first, __clen, __p, __type());
                __len -= __clen;
                __result += __clen;
            }
        }
        if (__pIt)
            *__pIt = __first;
        return __result;
    }

    template <typename Iterable>
    iterator __uninitialized_range_move2(Iterable __first, Iterable __last, iterator __result)
    {
        // difference_type __len = __n;
        if (__first == __last) return __result;
        typedef typename iterator_traits<Iterable>::value_type
        __iterable_valuetype;
        const bool __trivial = __is_trivial(__iterable_valuetype)
                               && __is_trivial(value_type);
        size_type __clen, __len;
        size_type __buf_size = this->__buf_size;
        pointer __p;
        if (!__trivial)
        {
            while (__first != __last)
            {
                __p = __result.base();
                __len = (__buf_size) - size_type(__result.m_cur - *(__result.m_node));
                for (__clen = 0; __clen != __len && __first != __last; ++__clen)
                {
                    _Construct(__p, std::move(*__first));
                    ++__p, ++__first;
                }
                // __len -= __clen;
                __result += __clen;
            }
        }
        else
        {
            using __type = typename conditional<
                           __are_same<__iterable_valuetype, value_type>::__value,
                           typename __is_pointer<Iterable>::__type,
                           __false_type>::type;
            while (__first != __last)
            {
                __p = __result.base();
                __len = __buf_size - size_type(__result.m_cur - *(__result.m_node));
                __first = this->__range_move_dispatch(
                              __first, __last, __p,
                              __len, &__clen, __type());
                // __len -= __clen;
                __result += __clen;
            }
        }
        return __result;
    }

    template <typename Iterable>
    iterator __uninitialized_move2(Iterable __first, Iterable __last, iterator __result)
    {
        return this->__uninitialized_range_move2<Iterable>(__first, __last, __result);
    }

    iterator __uninitialized_fill_n(iterator __result, size_type __n, const value_type& __val)
    {
        size_type __len = __n;
        size_type __clen;
        size_type __buf_size = this->__buf_size;
        pointer __p;
        while (__len > 0)
        {
            __p = __result.base();
            __clen = __buf_size - size_type(__result.m_cur - *(__result.m_node));
            __clen = min(__len, __clen);
            std::uninitialized_fill_n(__p, __clen, __val);
            __result += __clen;
            __len -= __clen;
        }
        return __result;
    }

    iterator __uninitialized_fill(iterator __first, iterator __last, const value_type& __val)
    {
        return this->__uninitialized_fill_n(__first, __last - __first, __val);
    }

private:

    void __initialize_map(size_type __n)
    {
        size_type __need_nodes = __n / this->__buf_size + 1;
        m_mapSize = max(size_type(8), __need_nodes + 2);
        m_map = __alloc_map(m_mapSize);
        map_ptr __nstart = m_map + (m_mapSize - __need_nodes) / 2;
        __create_buffer(__nstart, __nstart + __need_nodes);
        m_start.m_node = __nstart;
        m_start.m_cur = *(m_start.m_node);
        m_finish = m_start + __n;
    }

    void __drop_deque()
    {
        __destroy(m_start, m_finish);
        __dealloc_buffer(m_start.m_node,
                         m_finish.m_node + 1);
        __dealloc_map();
    }

    void __realloc_map(bool __add_front, size_type __add_nodes)
    {
        size_type __old_nodes = m_finish.m_node - m_start.m_node + 1;
        size_type __new_nodes = __old_nodes + __add_nodes;
        if (2 * __new_nodes < m_mapSize)
        {
            map_ptr __nstart = m_map + (m_mapSize - __new_nodes) / 2;
            if (__add_front)
                __nstart += __add_nodes;
            if (__nstart < m_start.m_node)
                std::copy(m_start.m_node, m_finish.m_node + 1, __nstart);
            else
                std::copy_backward(
                    m_start.m_node,
                    m_finish.m_node + 1,
                    __nstart + __old_nodes);
            m_start.m_node = __nstart;
            m_finish.m_node = __nstart + __old_nodes - 1;
        }
        else
        {
            size_type __new_map_size = m_mapSize + max(m_mapSize,
                                       __new_nodes - __old_nodes);
            map_ptr __new_map = __alloc_map(__new_map_size);
            map_ptr __nstart = __new_map + (__new_map_size - __new_nodes) / 2;
            if (__add_front)
                __nstart += __add_nodes;
            std::copy(m_start.m_node, m_finish.m_node + 1, __nstart);
            m_start.m_node = __nstart;
            m_finish.m_node = __nstart + __old_nodes - 1;
            __dealloc_map();
            m_map = __new_map;
            m_mapSize = __new_map_size;
        }
    }

    void __reserve_map_at_front(size_type __add_nodes)
    {
        if (__add_nodes > m_start.m_node - m_map)
            __realloc_map(true, __add_nodes);
    }

    void __reserve_map_at_back(size_type __add_nodes)
    {
        if (__add_nodes > m_mapSize - (m_finish.m_node - m_map + 1))
            __realloc_map(false, __add_nodes);
    }

    iterator __reserve_elements_at_front(size_type __add_elements)
    {
        size_type __left_res
            = m_start.m_cur - *(m_start.m_node);
        if (__add_elements > __left_res)
        {
            size_type __add_nodes
                = (__add_elements - __left_res - 1) / this->__buf_size + 1;
            __reserve_map_at_front(__add_nodes);
            __create_buffer(
                m_start.m_node - __add_nodes,
                m_start.m_node);
        }
        return m_start - __add_elements;
    }

    iterator __reserve_elements_at_back(size_type __add_elements)
    {
        size_type __right_res
            = this->__buf_size - (m_finish.m_cur - *(m_finish.m_node) + 1);
        if (__add_elements > __right_res)
        {
            size_type __add_nodes
                = (__add_elements - __right_res - 1) / this->__buf_size + 1;
            __reserve_map_at_back(__add_nodes);
            __create_buffer(
                m_finish.m_node + 1,
                m_finish.m_node + 1 + __add_nodes);
        }
        return m_finish + __add_elements;
    }

private:

    void __default_initialize(size_type __n)
    {
        __initialize_map(__n);
        std::__uninitialized_default_n(m_start, __n);
    }

    void __fill_initialize_dispatch(size_type __n, const value_type &__val, __true_type)
    {
        __initialize_map(__n);
        std::uninitialized_fill_n(m_start, __n, __val);
    }

    template <class Iterable>
    void __fill_initialize_dispatch(Iterable __first, Iterable __last, __false_type)
    {
        __initialize_map(distance(__first, __last));
        std::uninitialized_copy(__first, __last, m_start);
    }

    void __fill_initialize(size_type __n, const value_type &__val)
    {
        __fill_initialize_dispatch(__n, __val, __true_type());
    }

    template <class InputType>
    void __fill_initialize(InputType __first, InputType __last)
    {
        __fill_initialize_dispatch(__first, __last, typename __is_integer<InputType>::__type());
    }

private:
    void __insert_n_aux(iterator __pos, size_type __n, const value_type& __val)
    {
        size_type __left_elements = __pos - m_start;
        size_type __right_elements = m_finish - __pos;
        if (__left_elements < __right_elements)
        {
            iterator __new_start = __reserve_elements_at_front(__n);
            __pos = m_start + __left_elements;
            if (__left_elements > __n)
            {
                iterator __i = m_start + __n;
                this->__uninitialized_move(m_start, __i, __new_start);
                iterator __j = this->__move(__i, __pos, m_start);
                this->__fill_n(__j, __n, __val);
            }
            else
            {
                iterator __i = this->__uninitialized_move(
                                   m_start, __pos, __new_start);
                this->__uninitialized_fill(__i, m_start, __val);
                this->__fill_n(m_start, __left_elements, __val);
            }
            m_start = __new_start;
        }
        else
        {
            iterator __new_finish = __reserve_elements_at_back(__n);
            __pos = m_start + __left_elements;
            if (__right_elements > __n)
            {
                iterator __i = m_finish - __n;
                this->__uninitialized_move(__i, m_finish, m_finish);
                this->__move_backward(__pos, __i, m_finish);
                this->__fill_n(__pos, __n, __val);
            }
            else
            {
                iterator __i = __pos + __n;
                this->__uninitialized_move(__pos, m_finish, __i);
                this->__fill_n(__pos, __right_elements, __val);
                this->__uninitialized_fill(m_finish, __i, __val);
            }
            m_finish = __new_finish;
        }
    }

    template <typename Iterable>
    void __range_insert_aux(iterator __pos, Iterable __first, Iterable __last, size_type __n)
    {
        size_type __left_elements = __pos - m_start;
        size_type __right_elements = m_finish - __pos;
        if (__left_elements < __right_elements)
        {
            iterator __new_start = this->__reserve_elements_at_front(__n);
            __pos = m_start + __left_elements;
            if (__left_elements > __n)
            {
                iterator __i = m_start + __n;
                this->__uninitialized_move(m_start, __i, __new_start);
                iterator __j = this->__move(__i, __pos, m_start);
                this->__n_copy2<Iterable>(__first, __n, __j, nullptr);
            }
            else
            {
                iterator __i = this->__uninitialized_move(
                                   m_start, __pos, __new_start);
                Iterable __mid;
                this->__uninitialized_n_copy2<Iterable>(
                    __first, __n - __left_elements, __i, &__mid);
                this->__n_copy2<Iterable>(
                    __mid, __left_elements, m_start, nullptr);
            }
            m_start = __new_start;
        }
        else
        {
            iterator __new_finish = this->__reserve_elements_at_back(__n);
            __pos = m_start + __left_elements;
            if (__right_elements > __n)
            {
                iterator __i = m_finish - __n;
                this->__uninitialized_move(__i, m_finish, m_finish);
                this->__move_backward(__pos, __i, m_finish);
                this->__n_copy2<Iterable>(__first, __n, __pos, nullptr);
            }
            else
            {
                iterator __i = __pos + __n;
                this->__uninitialized_move(__pos, m_finish, __i);
                Iterable __mid;
                this->__n_copy2<Iterable>(__first, __right_elements, __pos, &__mid);
                this->__uninitialized_n_copy2<Iterable>(
                    __mid, __n - __right_elements, m_finish, nullptr);
            }
            m_finish = __new_finish;
        }
    }

    iterator __insert_one_aux(iterator __pos, const_reference __val)
    {
        if (__pos == m_start)
        {
            this->push_front(__val);
            return m_start;
        }
        if (__pos == m_finish)
        {
            this->push_back(__val);
            return m_finish;
        }
        difference_type __offset = __pos - m_start;
        iterator __i, __j, __pos1;
        if (__offset * 2 < size())
        {
            this->push_front(std::move(*m_start));
            __pos = m_start + (__offset + 1);
            __i = m_start;
            ++__i;
            __j = __i;
            ++__j;
            __pos1 = this->__move(__j, __pos, __i);
        }
        else
        {
            this->push_back(std::move(back()));
            __pos = m_start + __offset;
            __i = m_finish;
            --__i;
            __j = __i;
            --__j;
            __pos1 = this->__move_backward(__pos, __j, __i);
        }
        *__pos1 = __val;
        return __pos1;
    }

    iterator __erase_aux(iterator __first, iterator __last)
    {
        size_type __left_elements = __first - m_start;
        size_type __right_elements = m_finish - __last;
        if (__left_elements < __right_elements)
        {
            iterator __i = this->__move_backward(
                               m_start, __first, __last);
            this->__destroy(m_start, __i);
            this->__dealloc_buffer(
                m_start.m_node, __i.m_node);
            m_start = __i;
        }
        else
        {
            iterator __j = this->__move(
                               __last, m_finish, __first);
            this->__destroy(__j, m_finish);
            this->__dealloc_buffer(
                __j.m_node + 1, m_finish.m_node + 1);
            m_finish = __j;
        }
        return m_start + __left_elements;
    }

    void __assign_n_aux(size_type __n, const value_type& __val)
    {
        size_type __size = size();
        if (__n <= __size)
        {
            iterator __i = this->__fill_n(m_start, __n, __val);
            this->__destroy(__i, m_finish);
            this->__dealloc_buffer(
                __i.m_node + 1, m_finish.m_node + 1);
            m_finish = __i;
        }
        else
        {
            this->__fill_n(m_start, __size, __val);
            this->__insert_dispatch(
                m_finish, __n - __size, __val, __true_type());
        }
    }

    template <typename Iterable>
    void __range_assign_aux(Iterable __first, Iterable __last)
    {
        size_type __size = size();
        size_type __n = distance(__first, __last);
        if (__n <= __size)
        {
            iterator __i = this->__n_copy2<Iterable>(
                               __first, __n, m_start, nullptr);
            this->__destroy(__i, m_finish);
            this->__dealloc_buffer(
                __i.m_node + 1, m_finish.m_node + 1);
            m_finish = __i;
        }
        else
        {
            Iterable __mid;
            this->__n_copy2<Iterable>(
                __first, __size, m_start, &__mid);
            iterator __new_finish
                = this->__reserve_elements_at_back(__n - __size);
            this->__uninitialized_n_copy2<Iterable>(
                __mid, __n - __size, m_finish, nullptr);
            m_finish = __new_finish;
        }
    }

    iterator __erase(iterator __first, iterator __last)
    {
        if (__first == __last) return __first;
        if (__first == m_start)
        {
            this->__destroy(__first, __last);
            this->__dealloc_buffer(
                __first.m_node, __last.m_node);
            m_start = __last;
            return __last;
        }
        if (__last == m_finish)
        {
            this->__destroy(__first, __last);
            this->__dealloc_buffer(
                __first.m_node + 1, __last.m_node + 1);
            m_finish = __first;
            return __first;
        }
        return this->__erase_aux(__first, __last);
    }

private:

    void __insert_dispatch(iterator __pos, size_type __n, const value_type &__val, __true_type)
    {
        if (__n == 0) return;
        if (__n == 1)
        {
            this->__insert_one_aux(__pos, __val);
            return;
        }
        if (__pos.m_cur == m_start.m_cur)
        {
            iterator __new_start
                = this->__reserve_elements_at_front(__n);
            this->__uninitialized_fill_n(__new_start, __n, __val);
            m_start = __new_start;
        }
        else if (__pos.m_cur == m_finish.m_cur)
        {
            iterator __new_finish
                = this->__reserve_elements_at_back(__n);
            this->__uninitialized_fill_n(m_finish, __n, __val);
            m_finish = __new_finish;
        }
        else
            this->__insert_n_aux(__pos, __n, __val);
    }

    template <typename Iterable>
    void __insert_dispatch(iterator __pos, Iterable __first, Iterable __last, __false_type)
    {
        size_type __n = distance(__first, __last);
        if (__n == 0) return;
        if (__n == 1)
        {
            this->__insert_one_aux(__pos, *__first);
            return;
        }
        if (__pos == m_start)
        {
            iterator __new_start
                = this->__reserve_elements_at_front(__n);
            this->__uninitialized_n_copy2<Iterable>(
                __first, __n, __new_start, nullptr);
            m_start = __new_start;
        }
        else if (__pos == m_finish)
        {
            iterator __new_finish
                = this->__reserve_elements_at_back(__n);
            this->__uninitialized_n_copy2<Iterable>(
                __first, __n, m_finish, nullptr);
            m_finish = __new_finish;
        }
        else
            this->__range_insert_aux(__pos, __first, __last, __n);
    }

    void __assign_dispatch(size_type __n, const value_type& __val, __true_type)
    {
        this->__assign_n_aux(__n, __val);
    }

    template <typename Iterable>
    void __assign_dispatch(Iterable __first, Iterable __last, __false_type)
    {
        this->__range_assign_aux(__first, __last);
    }

    void __replace_dispatch(iterator __first, iterator __last, size_type __n, const value_type& __val, __true_type)
    {
        if (__first == __last)
            this->__insert_dispatch(__first, __n, __val, __true_type());
        else
        {
            difference_type __len = __last - __first;
            if (__len >= __n)
            {
                iterator __i = this->__uninitialized_fill_n(__first, __n, __val);
                erase(__i, __last);
            }
            else
            {
                iterator __j = this->__uninitialized_fill_n(__first, __len, __val);
                this->__insert_dispatch(__j, __n - __len, __val, __true_type());
            }
        }
    }

    template <typename Iterable>
    void __replace_dispatch(iterator __first, iterator __last, Iterable __ifirst, Iterable __ilast, __false_type)
    {
        if (__first == __last)
            this->__insert_dispatch(__first, __ifirst, __ilast, __false_type());
        else
        {
            difference_type __len = __last - __first;
            difference_type __n = distance(__ifirst, __ilast);
            if (__len >= __n)
            {
                iterator __i
                    = this->__uninitialized_n_copy2<Iterable>(
                          __ifirst, __n, __first, nullptr);
                this->erase(__i, __last);
            }
            else
            {
                Iterable __mid;
                iterator __j
                    = this->__uninitialized_n_copy2<Iterable>(
                          __ifirst, __len, __first, &__mid);
                if (__j == m_finish)
                {
                    iterator __new_finish
                        = this->__reserve_elements_at_back(__n - __len);
                    this->__uninitialized_n_copy2<Iterable>(
                        __mid, __n - __len, m_finish, nullptr);
                    m_finish = __new_finish;
                }
                else
                {
                    this->__range_insert_aux(
                        __j, __mid, __ilast, __n - __len);
                }
            }
        }
    }

public:
    void push_front(const value_type &val)
    {
        if (m_start.m_cur == *(m_start.m_node))
        {
            if (m_start.m_node == m_map)
                __realloc_map(true, 1);
            *(--m_start.m_node) = __alloc_buffer();
            m_start.m_cur = *(m_start.m_node) + this->__buf_size - 1;
        }
        else
            --(m_start.m_cur);
        _Construct(m_start.m_cur, val);
    }

    void push_front(value_type &&val)
    {
        if (m_start.m_cur == *(m_start.m_node))
        {
            if (m_start.m_node == m_map)
                __realloc_map(true, 1);
            *(--m_start.m_node) = __alloc_buffer();
            m_start.m_cur = *(m_start.m_node) + this->__buf_size - 1;
        }
        else
            --(m_start.m_cur);
        _Construct(m_start.m_cur, std::move(val));
    }

    void push_back(const value_type& val)
    {
        _Construct(m_finish.m_cur, val);
        if (m_finish.m_cur - *(m_finish.m_node) == this->__buf_size - 1)
        {
            if (m_finish.m_node - m_map == m_mapSize - 1)
                this->__realloc_map(false, 1);
            *(++m_finish.m_node) = __alloc_buffer();
            m_finish.m_cur = *(m_finish.m_node);
        }
        else ++(m_finish.m_cur);
    }

    void push_back(value_type&& val)
    {
        _Construct(m_finish.m_cur, std::move(val));
        if (m_finish.m_cur == *(m_finish.m_node) + this->__buf_size - 1)
        {
            if (m_finish.m_node == m_map + m_mapSize - 1)
                this->__realloc_map(false, 1);
            *(++m_finish.m_node) = __alloc_buffer();
            m_finish.m_cur = *(m_finish.m_node);
        }
        else ++(m_finish.m_cur);
    }

    void pop_front()
    {
        if (m_start != m_finish)
        {
            iterator __i = m_start + 1;
            _Destroy(m_start.m_cur);
            if (m_start.m_node != __i.m_node)
                __dealloc_buffer(m_start.m_node);
            m_start = __i;
        }
    }

    void pop_back()
    {
        if (m_start != m_finish)
        {
            iterator __i = m_finish - 1;
            _Destroy(__i.m_cur);
            if (m_finish.m_node != __i.m_node)
                __dealloc_buffer(m_finish.m_node);
            m_finish = __i;
        }
    }

    void insert(iterator pos, size_type n, const value_type& val)
    {
        this->__insert_dispatch(pos, n, val, __true_type());
    }

    iterator insert(iterator pos, const value_type& val)
    {
        return this->__insert_one_aux(pos, val);
    }

    template <typename InputType>
    void insert(iterator pos, InputType first, InputType last)
    {
        this->__insert_dispatch(pos, first, last, typename __is_integer<InputType>::__type());
    }

    void erase(iterator first, iterator last)
    {
        this->__erase(first, last);
    }

    iterator erase(iterator pos)
    {
        return this->__erase(pos, pos + 1);
    }

    void assign(size_type n, const value_type& val)
    {
        this->__assign_n_aux(n, val);
    }

    template <typename InputType>
    void assign(InputType first, InputType last)
    {
        this->__assign_dispatch(first, last, typename __is_integer<InputType>::__type());
    }

    void replace(iterator first, iterator last, size_type n, const value_type& val)
    {
        this->__replace_dispatch(first, last, n, val, __true_type());
    }

    template <typename Iterable>
    void replace(iterator first, iterator last, Iterable ifirst, Iterable ilast)
    {
        this->__replace_dispatch(first, last, ifirst, ilast, typename __is_integer<Iterable>::__type());
    }

public:

    Deque(size_type n = 0)
    {
        __default_initialize(n);
    }

    Deque(size_type n, const value_type &val)
    {
        __fill_initialize(n, val);
    }

    template <typename InputType>
    Deque(InputType first, InputType last)
    {
        __fill_initialize(first, last);
    }

    Deque(const Deque &dq)
    {
        if (!dq.empty())
            __fill_initialize(dq.cbegin(), dq.cend());
    }

    Deque &operator=(const Deque &dq)
    {
        if (&dq != this)
        {
            Deque::Deque(dq);
        }
        return *this;
    }

    Deque(Deque &&dq)
    {
        m_map = dq.m_map;
        m_mapSize = dq.m_mapSize;
        m_start = dq.m_start;
        m_finish = dq.m_finish;
        dq.m_map = nullptr;
        dq.m_mapSize = 0;
        dq.m_start.m_node = nullptr;
        dq.m_start.m_cur = nullptr;
        dq.m_finish = dq.m_start;
    }

    Deque &operator=(Deque &&dq)
    {
        if (&dq != this)
            Deque::Deque(dq);
        return *this;
    }

    ~Deque()
    {
        if (m_map)
        {
            __drop_deque();
        }
    }

public:
    size_type size() const
    {
        return m_finish == m_start ? 0 : m_finish - m_start;
    }

    size_type max_space() const
    {
        return size_type(-1);
    }

    bool empty() const
    {
        return m_finish == m_start;
    }

    void swap(Self &dq)
    {
        std::swap(m_start, dq.m_start);
        std::swap(m_finish, dq.m_finish);
        std::swap(m_map, dq.m_map);
        std::swap(m_mapSize, dq.m_mapSize);
    }

    void clear()
    {
        this->__destroy(m_start, m_finish);
        this->__dealloc_buffer(
            m_start.m_node + 1,
            m_finish.m_node + 1);
        m_finish = m_start;
    }

    reference operator [](size_type index)
    {
        return *(m_start + index);
    }

    reference at(size_type index)
    {
        if (index < 0 || index >= size())
            throw "out of range";
        return *(m_start + index);
    }

    const_reference front()
    {
        if (empty())
            throw "no elements";
        return *m_start;
    }

    const_reference back()
    {
        if (empty())
            throw "no elements";
        return *(m_finish - 1);
    }

public:

    iterator begin()
    {
        return m_start;
    }

    iterator end()
    {
        return m_finish;
    }

    const_iterator cbegin() const
    {
        return m_start;
    }

    const_iterator cend() const
    {
        return m_finish;
    }

    reverse_iterator rbegin()
    {
        return end();
    }

    reverse_iterator rend()
    {
        return begin();
    }

    const_reverse_iterator crbegin() const
    {
        return cend();
    }

    const_reverse_iterator crend() const
    {
        return cbegin();
    }
};

#endif