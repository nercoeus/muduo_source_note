// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOPTHREAD_H
#define MUDUO_NET_EVENTLOOPTHREAD_H

#include "muduo/base/Condition.h"
#include "muduo/base/Mutex.h"
#include "muduo/base/Thread.h"

// 一个线程
namespace muduo
{
namespace net
{

class EventLoop;
// 对 Thread 和 EventLoop 的封装
class EventLoopThread : noncopyable
{
public:
    typedef std::function<void(EventLoop *)> ThreadInitCallback;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const string &name = string());
    ~EventLoopThread();
    EventLoop *startLoop();

private:
    void threadFunc();
    // 对应的 EventLoop，每个线程对应一个
    EventLoop *loop_ GUARDED_BY(mutex_);
    bool exiting_;
    // 底层封装的线程
    Thread thread_;
    // 这里加入了 cond 来处理冲突
    MutexLock mutex_;
    Condition cond_ GUARDED_BY(mutex_);
    // 创建时传入
    ThreadInitCallback callback_;
};

} // namespace net
} // namespace muduo

#endif // MUDUO_NET_EVENTLOOPTHREAD_H
