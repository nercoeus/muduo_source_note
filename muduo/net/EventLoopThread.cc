// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/EventLoopThread.h"

#include "muduo/net/EventLoop.h"

using namespace muduo;
using namespace muduo::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback &cb,
                                 const string &name)
    : loop_(NULL),
      exiting_(false),
      thread_(std::bind(&EventLoopThread::threadFunc, this), name),   // 在 Thread 中运行 EventLoopThread::threadFunc
      mutex_(),
      cond_(mutex_),
      callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
    {
        // still a tiny chance to call destructed object, if threadFunc exits just now.
        // but when EventLoopThread destructs, usually programming is exiting anyway.
        loop_->quit();
        thread_.join();
    }
}

// 开始 loop
EventLoop *EventLoopThread::startLoop()
{
    assert(!thread_.started());
    thread_.start(); // 开始运行

    EventLoop *loop = NULL;
    {
        MutexLockGuard lock(mutex_);
        // 一直等到 Loop_ 创建完毕，这是在 Thread 中运行的 EventLoopThread::threadFunc 进行创建的
        while (loop_ == NULL)
        {
            // 等待信号量，创建完成后会发送信号量
            cond_.wait();
        }
        loop = loop_;
    }
    // 返回创建好的 Loop
    return loop;
}

// 线程运行函数
void EventLoopThread::threadFunc()
{
    // 创建 loop
    EventLoop loop;

    if (callback_)
    {
        // Eventloop 创建完成后进行回调
        callback_(&loop);
    }

    {
        MutexLockGuard lock(mutex_);
        // 加锁后赋值 loop_
        loop_ = &loop;
        cond_.notify();
    }
    loop.loop();
    //assert(exiting_);
    // 线程停止运行
    MutexLockGuard lock(mutex_);
    loop_ = NULL;
}
