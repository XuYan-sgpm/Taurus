#include "Deque.h"
using namespace std;

#ifndef __STACK_H__
#define __STACK_H__

template <class Tp>
class Stack
{
public:
    typedef Tp value_type;
    typedef Tp *pointer;
    typedef Tp &reference;
    typedef const Tp *const_pointer;
    typedef const Tp &const_reference;
    typedef ptrdiff_t difference_type;
    typedef size_t size_type;
    typedef Stack<Tp> Self;

protected:
    Deque<Tp> m_container;

public:
    Stack() {}
    Stack(const Stack& s):m_container(s.m_container) {}
    Stack(Stack&& s):m_container(s.m_container) {}
    ~Stack() {}

public:
    void push(const_reference val)
    {
        m_container.push_front(val);
    }

    void push(value_type&& val)
    {
        m_container.push_front(val);
    }

    void pop()
    {
        m_container.pop_front();
    }

    const_reference top()
    {
        return m_container.front();
    }

    bool empty()const
    {
        return m_container.empty();
    }

    size_t size()const
    {
        return m_container.size();
    }

    void swap(Self& s)
    {
        m_container.swap(s.m_container);
    }
};

#endif