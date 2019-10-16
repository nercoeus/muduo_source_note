// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_EVENTLOOPTHREADPOOL_H
#define MUDUO_NET_EVENTLOOPTHREADPOOL_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/Types.h"

#include <functional>
#include <memory>
#include <vector>

namespace muduo
{

namespace net
{

class EventLoop;
class EventLoopThread;
// 线程池
class EventLoopThreadPool : noncopyable
{
public:
    // 线程初始化回调函数
    typedef std::function<void(EventLoop *)> ThreadInitCallback;

    EventLoopThreadPool(EventLoop *baseLoop, const string &nameArg);
    ~EventLoopThreadPool();
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // valid after calling start()
    /// round-robin
    EventLoop *getNextLoop();

    /// with the same hash code, it will always return the same EventLoop
    // 根据 hash 随机获取一个 Loop
    EventLoop *getLoopForHash(size_t hashCode);

    std::vector<EventLoop *> getAllLoops();

    bool started() const
    {
        return started_;
    }

    const string &name() const
    {
        return name_;
    }

private:
    // 主循环的 EventLoop
    EventLoop *baseLoop_;
    string name_;    // name
    bool started_;   // 是否开始运行
    int numThreads_; // 线程数量
    int next_;       // 记录下一个使用的 Loop，暂时就是顺序使用
    // 所有 ThreadLoopThread
    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    // 所有 ThreadLoopThread 对应的 EventLoop
    std::vector<EventLoop *> loops_;
};

} // namespace net
} // namespace muduo

#endif // MUDUO_NET_EVENTLOOPTHREADPOOL_H
