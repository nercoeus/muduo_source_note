// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_ATOMIC_H
#define MUDUO_BASE_ATOMIC_H

#include "muduo/base/noncopyable.h"

#include <stdint.h>

namespace muduo
{

// 封装对 int 的原子操作，实现了加减设值操作
namespace detail
{
template <typename T>
// 不能 copy
class AtomicIntegerT : noncopyable
{
public:
    // __sync_val_compare_and_swap   ：读出旧值，旧值与存储值相同则写入
    // __sync_fetch_and_add  ：先获取值，再自加
    // __sync_lock_test_and_set  ：将value写入*ptr，对*ptr加锁，并返回操作之前*ptr的值
    // 以上3个函数有GCC提供，从汇编层面保证了操作的原子性，效率比加锁高。
    // 其余的所有函数均是调用这几个函数进行处理
    AtomicIntegerT()
        : value_(0)
    {
    }

    // uncomment if you need copying and assignment
    //
    // AtomicIntegerT(const AtomicIntegerT& that)
    //   : value_(that.get())
    // {}
    //
    // AtomicIntegerT& operator=(const AtomicIntegerT& that)
    // {
    //   getAndSet(that.get());
    //   return *this;
    // }

    T get()
    {
        // in gcc >= 4.7: __atomic_load_n(&value_, __ATOMIC_SEQ_CST)
        // GCC 提供的原子操作，汇编层面
        return __sync_val_compare_and_swap(&value_, 0, 0);
    }

    T getAndAdd(T x)
    {
        // in gcc >= 4.7: __atomic_fetch_add(&value_, x, __ATOMIC_SEQ_CST)
        // GCC 提供的原子操作，汇编层面
        return __sync_fetch_and_add(&value_, x);
    }
    // 先加再返回新值
    T addAndGet(T x)
    {
        return getAndAdd(x) + x;
    }
    // 加一再返回新值
    T incrementAndGet()
    {
        return addAndGet(1);
    }
    // 减一再返回新值
    T decrementAndGet()
    {
        return addAndGet(-1);
    }
    // 加 x 不返回
    void add(T x)
    {
        getAndAdd(x);
    }
    // 加一不返回
    void increment()
    {
        incrementAndGet();
    }
    // 减一不返回
    void decrement()
    {
        decrementAndGet();
    }
    // 直接设置一个新值
    T getAndSet(T newValue)
    {
        // in gcc >= 4.7: __atomic_exchange_n(&value, newValue, __ATOMIC_SEQ_CST)
        // GCC 提供的原子操作，汇编层面
        return __sync_lock_test_and_set(&value_, newValue);
    }

private:
    // 原子操作的值，使用 volatile 每次都从内存中进行读取
    volatile T value_;
};
} // namespace detail

// 两个版本，32 位 64 位
typedef detail::AtomicIntegerT<int32_t> AtomicInt32;
typedef detail::AtomicIntegerT<int64_t> AtomicInt64;

} // namespace muduo

#endif // MUDUO_BASE_ATOMIC_H
