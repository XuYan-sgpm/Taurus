#include "Deque.h"
using namespace std;

#ifndef __QUEUE_H__
#define __QUEUE_H__

template <class Tp>
class Queue
{
public:
    typedef Tp value_type;
    typedef Tp *pointer;
    typedef Tp &reference;
    typedef const Tp *const_pointer;
    typedef const Tp &const_reference;
    typedef ptrdiff_t difference_type;
    typedef size_t size_type;
    typedef Queue<Tp> Self;

protected:
    Deque<Tp> m_container;

public:
    Queue() {}
    Queue(const Queue& q):m_container(q.m_container) {}
    Queue(Queue&& q):m_container(q.m_container) {}
    ~Queue() {}

public:
    void push(const_reference val)
    {
        m_container.push_back(val);
    }

    void push(value_type&& val)
    {
        m_container.push_back(val);
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

    void swap(Self& q)
    {
        m_container.swap(q.m_container);
    }
};

#endif