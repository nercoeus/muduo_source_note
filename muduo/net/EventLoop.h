// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOP_H
#define MUDUO_NET_EVENTLOOP_H

#include <atomic>
#include <functional>
#include <vector>

#include <boost/any.hpp>

#include "muduo/base/Mutex.h"
#include "muduo/base/CurrentThread.h"
#include "muduo/base/Timestamp.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/TimerId.h"

namespace muduo
{
namespace net
{

class Channel;
class Poller;
class TimerQueue;

///
/// Reactor, at most one per thread.
///
/// This is an interface class, so don't expose too much details.
// 关于 事件循环的接口，不同系统有不同的是实现方式 poll 或者 epoll
class EventLoop : noncopyable
{
public:
    typedef std::function<void()> Functor;

    EventLoop();
    ~EventLoop(); // force out-line dtor, for std::unique_ptr members.

    ///
    /// Loops forever.
    ///
    /// Must be called in the same thread as creation of the object.
    ///
    // 必须在这个 eventloop 所在的线程中运行
    void loop();

    /// Quits loop.
    ///
    /// This is not 100% thread safe, if you call through a raw pointer,
    /// better to call through shared_ptr<EventLoop> for 100% safety.
    void quit();

    ///
    /// Time when poll returns, usually means data arrival.
    ///
    // poll 返回时间，一般是因为收到数据返回
    Timestamp pollReturnTime() const { return pollReturnTime_; }

    int64_t iteration() const { return iteration_; }

    /// Runs callback immediately in the loop thread.
    /// It wakes up the loop, and run the cb.
    /// If in the same loop thread, cb is run within the function.
    /// Safe to call from other threads.
    // 立即在线程中运行回调函数
    void runInLoop(Functor cb);
    /// Queues callback in the loop thread.
    /// Runs after finish pooling.
    /// Safe to call from other threads.
    void queueInLoop(Functor cb);

    size_t queueSize() const;

    // timers

    ///
    /// Runs callback at 'time'.
    /// Safe to call from other threads.
    ///
    TimerId runAt(Timestamp time, TimerCallback cb);
    ///
    /// Runs callback after @c delay seconds.
    /// Safe to call from other threads.
    ///
    TimerId runAfter(double delay, TimerCallback cb);
    ///
    /// Runs callback every @c interval seconds.
    /// Safe to call from other threads.
    ///
    TimerId runEvery(double interval, TimerCallback cb);
    ///
    /// Cancels the timer.
    /// Safe to call from other threads.
    ///
    void cancel(TimerId timerId);

    // internal usage
    void wakeup();
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // pid_t threadId() const { return threadId_; }
    // 判断是否处当前线程
    void assertInLoopThread()
    {
        if (!isInLoopThread())
        {
            abortNotInLoopThread();
        }
    }
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    // bool callingPendingFunctors() const { return callingPendingFunctors_; }
    bool eventHandling() const { return eventHandling_; }

    void setContext(const boost::any &context)
    {
        context_ = context;
    }

    const boost::any &getContext() const
    {
        return context_;
    }

    boost::any *getMutableContext()
    {
        return &context_;
    }

    static EventLoop *getEventLoopOfCurrentThread();

private:
    void abortNotInLoopThread();
    void handleRead(); // waked up
    void doPendingFunctors();

    void printActiveChannels() const; // DEBUG
    typedef std::vector<Channel *> ChannelList;
    // looping
    bool looping_; /* atomic */
    std::atomic<bool> quit_;
    // 正在处理事件
    bool eventHandling_;          /* atomic */
    // 正在运行函数队列
    bool callingPendingFunctors_; /* atomic */
    int64_t iteration_;           // 循环次数
    const pid_t threadId_;        // 线程 Id
    Timestamp pollReturnTime_;
    // EventLoop 中的 Poll
    std::unique_ptr<Poller> poller_;
    // 事件队列
    std::unique_ptr<TimerQueue> timerQueue_;
    // 用来接收时事件通知的 fd，对 EventLoop 进行管理，使用了 eventfd
    int wakeupFd_;
    // unlike in TimerQueue, which is an internal class,
    // we don't expose Channel to client.
    std::unique_ptr<Channel> wakeupChannel_;
    boost::any context_;

    // scratch variables
    ChannelList activeChannels_;    // 当前 Eventloop 持有的 channel
    Channel *currentActiveChannel_; // 当前活动 channels
    // loop 需要全局锁
    mutable MutexLock mutex_;
    // 等待在本线程上执行的函数
    std::vector<Functor> pendingFunctors_ GUARDED_BY(mutex_);
};

} // namespace net
} // namespace muduo

#endif // MUDUO_NET_EVENTLOOP_H
