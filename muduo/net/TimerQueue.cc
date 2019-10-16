// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

#include "muduo/net/TimerQueue.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/Timer.h"
#include "muduo/net/TimerId.h"

#include <sys/timerfd.h>
#include <unistd.h>

// 这里使用了 timerfd 库来实现定时器


namespace muduo
{
namespace net
{
namespace detail
{
// 创建对应的 timefd
int createTimerfd()
{
    // 调用 timerfd_create 进行创建（Linux 特有）
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC,
                                   TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerfd < 0)
    {
        LOG_SYSFATAL << "Failed in timerfd_create";
    }
    return timerfd;
}

// 计算超时时刻与当前时间的时间差
struct timespec howMuchTimeFromNow(Timestamp when)
{
    int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();
    // 精确度没有设置那么高，所以小于100ms时都置为100
    if (microseconds < 100)
    {
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(
        microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
        (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
    return ts;
}

// 处理超时事件。超时后，timerfd 变为可读
void readTimerfd(int timerfd, Timestamp now)
{
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
    if (n != sizeof howmany)
    {
        LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
    }
}

// 重新设定 Poll 触发时间
void resetTimerfd(int timerfd, Timestamp expiration)
{
    // wake up loop by timerfd_settime()
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memZero(&newValue, sizeof newValue);
    memZero(&oldValue, sizeof oldValue);
    // 新触发时间
    newValue.it_value = howMuchTimeFromNow(expiration);
    // 为 timefd 设置新的时间
    int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
    if (ret)
    {
        LOG_SYSERR << "timerfd_settime()";
    }
}

} // namespace detail
} // namespace net
} // namespace muduo

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop *loop)
    : loop_(loop),
      timerfd_(createTimerfd()),       // 创建时间时间的 fd
      timerfdChannel_(loop, timerfd_), // 创建对应的 channel
      timers_(),                       // 空的 List
      callingExpiredTimers_(false)
{
    // 注册处理函数
    timerfdChannel_.setReadCallback(
        std::bind(&TimerQueue::handleRead, this));
    // we are always reading the timerfd, we disarm it with timerfd_settime.
    // 从 channel 中读取数据
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
    // 先删除 channel
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    // close 对应的描述符
    ::close(timerfd_);
    // do not remove channel, since we're in EventLoop::dtor();
    for (const Entry &timer : timers_)
    {
        // 释放所有时间事件
        delete timer.second;
    }
}

// 添加一个定时事件
TimerId TimerQueue::addTimer(TimerCallback cb,
                             Timestamp when,
                             double interval)
{
    Timer *timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop(
        std::bind(&TimerQueue::addTimerInLoop, this, timer));
    return TimerId(timer, timer->sequence());
}

// 取消一个定时事件
void TimerQueue::cancel(TimerId timerId)
{
    loop_->runInLoop(
        std::bind(&TimerQueue::cancelInLoop, this, timerId));
}

// 添加一个定时事件，线程中执行
void TimerQueue::addTimerInLoop(Timer *timer)
{
    loop_->assertInLoopThread();
    // 插入事件，可能修改 timerfd 触发时间
    bool earliestChanged = insert(timer);

    if (earliestChanged)
    {
        // 需要提前返回，就更新 Poll 中的定时时间
        resetTimerfd(timerfd_, timer->expiration());
    }
}

// 取消一个定时事件，线程中执行
void TimerQueue::cancelInLoop(TimerId timerId)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    // 要取消的定时器 timer
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    // 查找这个 timer
    ActiveTimerSet::iterator it = activeTimers_.find(timer);
    if (it != activeTimers_.end())
    {
        // 删除的事件正在执行，从 timers 中移除
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        assert(n == 1);
        (void)n;
        delete it->first; // FIXME: no delete please
        // 从 active 中删除
        activeTimers_.erase(it);
    }
    else if (callingExpiredTimers_)
    {
        // 正在执行事件的回调函数，加入 cancelingTimers_ 中
        // 这么做是因为正在处理过期事件时，会将事件从 activeTimers_ 删除，还存在于执行队列中
        // 这就导致了该事件可能是可循环的，当时间执行完再判断是否重新添加到 timerfd 中时要排除 cancelingTimers_ 中的事件
        cancelingTimers_.insert(timer);
    }
    assert(timers_.size() == activeTimers_.size());
}

// 时间到时处理事件
void TimerQueue::handleRead()
{
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    // 读 timerfd
    readTimerfd(timerfd_, now);
    // 获取可执行事件
    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    // safe to callback outside critical section
    for (const Entry &it : expired)
    {
        // 这里回调定时器处理函数
        it.second->run(); // run()->callback()
    }
    callingExpiredTimers_ = false;
    // 最后判断是不是可重复的，可重复的会重新添加到队列中
    reset(expired, now);
}

// rvo即Return Value Optimization
// 是一种编译器优化技术，可以把通过函数返回创建的临时对象给”去掉”
// 然后达到少调用拷贝构造的操作，从而提高性能
// 返回需要执行的事件集合
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    assert(timers_.size() == activeTimers_.size());
    std::vector<Entry> expired;
    Entry sentry(now, reinterpret_cast<Timer *>(UINTPTR_MAX));
    TimerList::iterator end = timers_.lower_bound(sentry);
    assert(end == timers_.end() || now < end->first);
    // 将 timers 中到期的事件都取出来
    std::copy(timers_.begin(), end, back_inserter(expired));
    // 从 timers_ 删除过期事件
    timers_.erase(timers_.begin(), end);
    // 在 activeTimers_ 也删除
    for (const Entry &it : expired)
    {
        ActiveTimer timer(it.second, it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        assert(n == 1);
        (void)n;
    }

    assert(timers_.size() == activeTimers_.size());
    return expired;
}

// 重新设置时间计时器
void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now)
{
    Timestamp nextExpire;

    for (const Entry &it : expired)
    {
        ActiveTimer timer(it.second, it.second->sequence());
        // 如果是重复的定时器并且不在 cancelingTimers_ 集合中，则重启该定时器
        if (it.second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end())
        {
            it.second->restart(now);
            insert(it.second);
        }
        else
        {
            // FIXME move to a free list
            delete it.second; // FIXME: no delete please
        }
    }

    if (!timers_.empty())
    {
        // 获取下次到期时间
        nextExpire = timers_.begin()->second->expiration();
    }

    if (nextExpire.valid())
    {
        // 调用 resetTimerfd 重置超时时间
        resetTimerfd(timerfd_, nextExpire);
    }
}

// 插入新的定时事件
bool TimerQueue::insert(Timer *timer)
{
    loop_->assertInLoopThread();
    assert(timers_.size() == activeTimers_.size());
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    TimerList::iterator it = timers_.begin();
    // 如果 timers_ 为空或者 when 小于 timers_ 中的最早到期时间
    if (it == timers_.end() || when < it->first)
    {
        earliestChanged = true;
    }
    {
        // 插入到 timers_ 中
        std::pair<TimerList::iterator, bool> result = timers_.insert(Entry(when, timer));
        assert(result.second);
        (void)result;
    }
    {
        // 插入到 activeTimers_ 中
        std::pair<ActiveTimerSet::iterator, bool> result = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
        assert(result.second);
        (void)result;
    }

    assert(timers_.size() == activeTimers_.size());
    return earliestChanged;
}
