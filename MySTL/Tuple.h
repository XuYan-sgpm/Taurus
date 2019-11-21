#include <malloc.h>

using namespace std;

#ifndef __TUPLE_H__
#define __TUPLE_H__

template <typename Tp>
struct __add_c_ref
{
    typedef const Tp &type;
};

template <typename Tp>
struct __add_c_ref<Tp&>
{
    typedef Tp &type;
};

template <typename Tp>
struct __add_ref
{
    typedef Tp &type;
};

template <typename Tp>
struct __add_ref<Tp&>
{
    typedef Tp &type;
};

template <int _Index, typename... _Args>
struct _Tuple_impl;

template <int _Index>
struct _Tuple_impl<_Index> {};

template <int _Index, typename _Head, typename... _Tail>
struct _Tuple_impl<_Index, _Head, _Tail...>
    : public _Tuple_impl<_Index + 1, _Tail...>
{
    typedef _Tuple_impl<_Index + 1, _Tail...> _Inherited;
    _Head _m_head;

    _Inherited &_tail()
    {
        return *this;
    }

    const _Inherited& _tail()const
    {
        return *this;
    }

    _Tuple_impl():_Inherited(), _m_head() {}

    explicit _Tuple_impl(typename __add_c_ref<_Head>::type _head,
                         typename __add_c_ref<_Tail>::type... _tail)
        :_Inherited(_tail...), _m_head(_head) {}

    template <typename... _Args>
    _Tuple_impl(const _Tuple_impl<_Index, _Args...>& _impl)
        :_Inherited(_impl._tail()), _m_head(_impl._m_head) {}

    template <typename... _Args>
    _Tuple_impl& operator =(const _Tuple_impl<_Index, _Args...>& _impl)
    {
        _m_head = _impl._m_head;
        _Tuple_impl::_Tuple_impl(_impl._tail());
        return *this;
    }
};

template <typename... _Args>
class Tuple : public _Tuple_impl<0, _Args...>
{
    typedef _Tuple_impl<0, _Args...> _Inherited;

public:
    Tuple():_Inherited() {}

    explicit Tuple(typename __add_c_ref<_Args>::type... _args)
        : _Inherited(_args...) {}

    template <typename... _Elements>
    Tuple(const Tuple<_Elements...>& t) : _Inherited(t) {}

    template <typename... _Elements>
    Tuple& operator =(const Tuple<_Elements...>& t)
    {
        Tuple::Tuple(t);
        return *this;
    }
};

template<> class Tuple<> { };

template <int _Index, typename _Tp>
struct TupleElements;

template <int _Index, typename _Head, typename... _Args>
struct TupleElements<_Index, Tuple<_Head, _Args...> >
    : TupleElements<_Index - 1, Tuple<_Args...> > { };

template <typename _Head, typename... _Args>
struct TupleElements<0, Tuple<_Head, _Args...> >
{
    typedef _Head type;
};

template <typename... _Args>
inline int TupleSize()
{
    return sizeof...(_Args);
}

template <typename... _Args>
inline int TupleSize(const Tuple<_Args...>& t)
{
    return sizeof...(_Args);
}

template <typename... _Args>
struct _Tuple_size;

template <typename... _Args>
struct _Tuple_size<Tuple<_Args...> >
{
    const static int value = sizeof...(_Args);
};

template <int _Index, typename _Head, typename... _Args>
inline typename __add_ref<_Head>::type
__get_aux(_Tuple_impl<_Index, _Head, _Args...>& __impl)
{
    return __impl._m_head;
}

template <int _Index, typename _Head, typename... _Args>
inline typename __add_c_ref<_Head>::type
__get_aux(const _Tuple_impl<_Index, _Head, _Args...>& __impl)
{
    return __impl._m_head;
}

template <int _Index, typename... _Args>
inline typename __add_ref<
typename TupleElements<_Index, Tuple<_Args...> >::type>::type
get(Tuple<_Args...>& t)
{
    return __get_aux<_Index>(t);
}

template <int _Index, typename... _Args>
inline typename __add_c_ref<
typename TupleElements<_Index, Tuple<_Args...> >::type>::type
get(const Tuple<_Args...>& t)
{
    return __get_aux<_Index>(t);
}

template <int _Dis, int _I, int _J, typename _T, typename _U>
struct __compare_tuple
{
    static bool __equal(const _T& __t, const _U& __u)
    {
        static_assert(_Dis == 0, "two tuple size is not equal");
        return false;
    }

    static bool __less(const _T& __t, const _U& __u)
    {
        static_assert(_Dis == 0, "two tuple size is not equal");
        return false;
    }
};

template <int _I, int _J, typename _T, typename _U>
struct __compare_tuple<0, _I, _J, _T, _U>
{
    static bool __equal(const _T& __t, const _U& __u)
    {
        return get<_I>(__t) == get<_I>(__u)
               && __compare_tuple<0, _I + 1, _J, _T, _U>::__equal(__t, __u);
    }

    static bool __less(const _T& __t, const _U& __u)
    {
        return get<_I>(__t) < get<_I>(__u)
               || __compare_tuple<0, _I + 1, _J, _T, _U>::__less(__t, __u);
    }
};

template <int _I, typename _T, typename _U>
struct __compare_tuple<0, _I, _I, _T, _U>
{
    static bool __equal(const _T& __t, const _U& __u)
    {
        return true;
    }

    static bool __less(const _T& __t, const _U& __u)
    {
        return false;
    }
};

template <int _First, int _Last, typename... _TElements, typename... _UElements>
bool TupleLess(const Tuple<_TElements...>& t, const Tuple<_UElements...>& u)
{
    return __compare_tuple<
           _Tuple_size<Tuple<_TElements...>>::value
           - _Tuple_size<Tuple<_UElements...>>::value,
           _First, _Last,
           Tuple<_TElements...>,
           Tuple<_UElements...> >::__less(t, u);
}

template <int _First, int _Last, typename... _TElements, typename... _UElements>
bool TupleGreater(const Tuple<_TElements...>& t, const Tuple<_UElements...>& u)
{
    return __compare_tuple<
           _Tuple_size<Tuple<_TElements...>>::value
           - _Tuple_size<Tuple<_UElements...>>::value,
           _First, _Last,
           Tuple<_UElements...>,
           Tuple<_TElements...> >::__less(u, t);
}

template <int _First, int _Last, typename... _TElements, typename... _UElements>
bool TupleEqual(const Tuple<_TElements...>& t, const Tuple<_UElements...>& u)
{
    return __compare_tuple<
           _Tuple_size<Tuple<_TElements...> >::value
           - _Tuple_size<Tuple<_UElements...> >::value,
           _First, _Last,
           Tuple<_TElements...>,
           Tuple<_UElements...> >::__equal(t, u);
}

template <typename... _TElements, typename... _UElements>
bool operator >(const Tuple<_TElements...>& t, const Tuple<_UElements...>& u)
{
    return TupleGreater<0, _Tuple_size<Tuple<_TElements...> >::value>(t, u);
}

template <typename... _TElements, typename... _UElements>
bool operator <(const Tuple<_TElements...>& t, const Tuple<_UElements...>& u)
{
    return TupleLess<0, _Tuple_size<Tuple<_TElements...> >::value>(t, u);
}

template <typename... _TElements, typename... _UElements>
bool operator ==(const Tuple<_TElements...>& t, const Tuple<_UElements...>& u)
{
    return TupleEqual<0, _Tuple_size<Tuple<_TElements...> >::value>(t, u);
}

template <typename... _TElements, typename... _UElements>
bool operator >=(const Tuple<_TElements...>& t, const Tuple<_UElements...>& u)
{
    return !TupleLess<0, _Tuple_size<Tuple<_TElements...> >::value>(t, u);
}

template <typename... _TElements, typename... _UElements>
bool operator <=(const Tuple<_TElements...>& t, const Tuple<_UElements...>& u)
{
    return !TupleGreater<0, _Tuple_size<Tuple<_TElements...> >::value>(t, u);
}

template <typename _Tp>
struct __remove_reference_wrapper
{
    typedef _Tp __type;
};

template <typename _Tp>
struct __remove_reference_wrapper<reference_wrapper<_Tp> >
{
    typedef _Tp &__type;
};

template <typename _Tp>
struct __remove_reference_wrapper<const reference_wrapper<_Tp> >
{
    typedef _Tp &__type;
};

template <typename... _Args>
inline Tuple<typename __remove_reference_wrapper<_Args>::__type...>
MakeTuple(_Args... args)
{
    typedef Tuple<typename __remove_reference_wrapper<_Args>::__type...> result_type;
    return result_type(args...);
}

#endif