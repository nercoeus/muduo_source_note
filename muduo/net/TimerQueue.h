// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMERQUEUE_H
#define MUDUO_NET_TIMERQUEUE_H

#include <set>
#include <vector>

#include "muduo/base/Mutex.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/Channel.h"

namespace muduo
{
namespace net
{

class EventLoop;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
/// 尽量但不保证在准确时间执行事件
class TimerQueue : noncopyable
{
public:
    explicit TimerQueue(EventLoop *loop);
    ~TimerQueue();

    ///
    /// Schedules the callback to be run at given time,
    /// repeats if @c interval > 0.0.
    ///
    /// Must be thread safe. Usually be called from other threads.
    // 必须是线程安全的，可以跨线程调用。通常情况下被其它线程调用。
    TimerId addTimer(TimerCallback cb,
                     Timestamp when,
                     double interval);

    void cancel(TimerId timerId);

private:
    // FIXME: use unique_ptr<Timer> instead of raw pointers.
    // This requires heterogeneous comparison lookup (N3465) from C++14
    // so that we can find an T* in a set<unique_ptr<T>>.
    // 两种类型的 set，一种按时间戳排序，一种按Timer的地址排序
    // 实际上，这两个 set 保存的是相同的定时器列表
    typedef std::pair<Timestamp, Timer *> Entry;
    typedef std::set<Entry> TimerList;
    typedef std::pair<Timer *, int64_t> ActiveTimer;
    typedef std::set<ActiveTimer> ActiveTimerSet;

    void addTimerInLoop(Timer *timer);
    void cancelInLoop(TimerId timerId);
    // called when timerfd alarms
    // 当 timefd 触发时
    void handleRead();
    // move out all expired timers
    std::vector<Entry> getExpired(Timestamp now);
    void reset(const std::vector<Entry> &expired, Timestamp now);

    bool insert(Timer *timer);

    EventLoop *loop_;        // 持有这个队列的 EventLoop
    const int timerfd_;      // 注册到 Poll 中和 timeEvent 相关的 fd
    Channel timerfdChannel_; // timefd 对应的 channel
    // Timer list sorted by expiration
    // 根据到期事件排序的已经事件 List
    TimerList timers_;

    // for cancel()
    // activeTimers_是按对象地址排序的事件，存储的内容和 timers_ 完全一样
    ActiveTimerSet activeTimers_;
    bool callingExpiredTimers_; /* atomic */ // 是否正在处理超时事件
    ActiveTimerSet cancelingTimers_;         // 保存的是被取消的定时器
};

} // namespace net
} // namespace muduo
#endif // MUDUO_NET_TIMERQUEUE_H
