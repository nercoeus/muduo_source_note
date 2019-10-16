// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TIMERID_H
#define MUDUO_NET_TIMERID_H

#include "muduo/base/copyable.h"

namespace muduo
{
namespace net
{

class Timer;

///
/// An opaque identifier, for canceling Timer.
///
class TimerId : public muduo::copyable
{
public:
    TimerId()
        : timer_(NULL),
          sequence_(0)
    {
    }

    TimerId(Timer *timer, int64_t seq)
        : timer_(timer),
          sequence_(seq)
    {
    }

    // default copy-ctor, dtor and assignment are okay
    // 设置 TimeQueue 为友元，操作内部的数据
    friend class TimerQueue;

private:
    // 定时事件的指针
    Timer *timer_;
    // 在队列中的位置
    int64_t sequence_;
};

} // namespace net
} // namespace muduo

#endif // MUDUO_NET_TIMERID_H
