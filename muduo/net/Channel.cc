// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/base/Logging.h"
#include "muduo/net/Channel.h"
#include "muduo/net/EventLoop.h"

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;
// 构造函数
Channel::Channel(EventLoop *loop, int fd__)
    : loop_(loop),
      fd_(fd__),
      events_(0),   // 默认 kNoneEvent
      revents_(0),
      index_(-1),
      logHup_(true),
      tied_(false),  // 默认不进行关联，accept 的 channel 不关联，TcpConnection 的 channel 进行关联
      eventHandling_(false),
      addedToLoop_(false)
{
}

Channel::~Channel()
{
    // 还在处理事件，不能析构
    assert(!eventHandling_);
    assert(!addedToLoop_);
    if (loop_->isInLoopThread())
    {
        assert(!loop_->hasChannel(this));
    }
}

// 关联 obj，一般是 TcpConnection
void Channel::tie(const std::shared_ptr<void> &obj)
{
    // 进行绑定
    tie_ = obj;
    tied_ = true;
}

// channel::update 调用 EventLoop::updateChannel
void Channel::update()
{
    addedToLoop_ = true;
    // 更新在 loop 中的 channel 这里其实 loop 的 updateChannel 使用的是 poll 的 update
    // 使用的就是 events 和 fd
    loop_->updateChannel(this);
}

// channel::update 调用 EventLoop::removeChannel
void Channel::remove()
{
    assert(isNoneEvent());
    addedToLoop_ = false;
    // 从 loop 中删除这个 channel
    loop_->removeChannel(this);
}

// 事件回调处理（这个函数不用注册，直接在 EventLoop 中调用）
void Channel::handleEvent(Timestamp receiveTime)
{
    std::shared_ptr<void> guard;
    if (tied_)
    {
        // 对于 TcpConnection 需要进行 Lock
        guard = tie_.lock();
        if (guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
        // 对于 Acceptor 不需要进行 Lock，因为肯定不会发生冲突
        handleEventWithGuard(receiveTime);
    }
}

// 执行触发操作
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    // 事件处理中
    eventHandling_ = true;
    LOG_TRACE << reventsToString();
    // 分别调用各种回调函数即可
    if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
    {
        if (logHup_)
        {
            LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLHUP";
        }
        if (closeCallback_)
            // 关闭事件
            closeCallback_();
    }

    if (revents_ & POLLNVAL)
    {
        LOG_WARN << "fd = " << fd_ << " Channel::handle_event() POLLNVAL";
    }

    if (revents_ & (POLLERR | POLLNVAL))
    {
        if (errorCallback_)
            // 出错事件
            errorCallback_();
    }
    if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))
    {
        if (readCallback_)
            // 可读事件
            readCallback_(receiveTime);
    }
    if (revents_ & POLLOUT)
    {
        if (writeCallback_)
            // 可写事件
            writeCallback_();
    }
    // 事件处理完毕
    eventHandling_ = false;
}

// 下面的函数用来打印 fd 状态
string Channel::reventsToString() const
{
    return eventsToString(fd_, revents_);
}

string Channel::eventsToString() const
{
    return eventsToString(fd_, events_);
}

string Channel::eventsToString(int fd, int ev)
{
    std::ostringstream oss;
    oss << fd << ": ";
    if (ev & POLLIN)
        oss << "IN ";
    if (ev & POLLPRI)
        oss << "PRI ";
    if (ev & POLLOUT)
        oss << "OUT ";
    if (ev & POLLHUP)
        oss << "HUP ";
    if (ev & POLLRDHUP)
        oss << "RDHUP ";
    if (ev & POLLERR)
        oss << "ERR ";
    if (ev & POLLNVAL)
        oss << "NVAL ";

    return oss.str();
}
