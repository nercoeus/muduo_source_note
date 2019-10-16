// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/Timestamp.h"

#include <functional>
#include <memory>

namespace muduo
{
namespace net
{

class EventLoop;

///
/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor.
/// The file descriptor could be a socket,
/// an eventfd, a timerfd, or a signalfd
// 每个 channel 绑定一个 fd，封装了 fd 在 Poll 中的一系列操作
// 以及这个 fd 触发时的各种回调函数
// 暂时仅供 acceptor TcpConnection 进行使用
class Channel : noncopyable
{
public:
    // 这两种回调函数是不一样的，存在一个事件参数
    typedef std::function<void()> EventCallback;
    typedef std::function<void(Timestamp)> ReadEventCallback;
    // 使用 EventLoop 和 fd 进行构造
    Channel(EventLoop *loop, int fd);
    ~Channel();
    // 有关心的事件触发时，使用 handleEvent 进行判断是什么类型的，即需要调用哪一个函数
    void handleEvent(Timestamp receiveTime);
    // 对四种函数进行注册
    void setReadCallback(ReadEventCallback cb)
    {
        readCallback_ = std::move(cb);
    }
    void setWriteCallback(EventCallback cb)
    {
        writeCallback_ = std::move(cb);
    }
    void setCloseCallback(EventCallback cb)
    {
        closeCallback_ = std::move(cb);
    }
    void setErrorCallback(EventCallback cb)
    {
        errorCallback_ = std::move(cb);
    }

    /// Tie this channel to the owner object managed by shared_ptr,
    /// prevent the owner object being destroyed in handleEvent.
    // 将此 channel 绑定到 shared_ptr 管理的所有者对象，
    // 防止在 handleEvent 中销毁所有者对象。
    void tie(const std::shared_ptr<void> &);
    // 获取 channel 中的字段
    int fd() const { return fd_; }
    int events() const { return events_; }
    void set_revents(int revt) { revents_ = revt; } // used by pollers 这个接口仅仅由 poll 使用
    // int revents() const { return revents_; }
    // 设置为什么事件都不关心
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    // 关心可读事件到 poll
    void enableReading()
    {
        events_ |= kReadEvent;
        update();
    }
    // 删除可读事件到 poll
    void disableReading()
    {
        events_ &= ~kReadEvent;
        update();
    }
    // 关心可写事件到 poll
    void enableWriting()
    {
        events_ |= kWriteEvent;
        update();
    }
    // 删除可写事件到 poll
    void disableWriting()
    {
        events_ &= ~kWriteEvent;
        update();
    }
    // 删除所有事件到 poll
    void disableAll()
    {
        events_ = kNoneEvent;
        update();
    }
    // 判断这个套接字所关心的事件
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // for Poller poll 使用的接口
    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    // for debug 用来查看 fd 的状态变化
    string reventsToString() const;
    string eventsToString() const;

    void doNotLogHup() { logHup_ = false; }
    // 返回这个线程的 Loop
    EventLoop *ownerLoop() { return loop_; }
    void remove();

private:
    static string eventsToString(int fd, int ev);
    // 把 channel 添加/修改 至 poll
    void update();
    void handleEventWithGuard(Timestamp receiveTime);
    // 三种状态
    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop *loop_; // channel 所属的 eventloop,即对应的 accept 的 EventLoop 或者 TcpConnection 的 EventLoop
    const int fd_;    // channel 所持有的 fd
    int events_;      // channel 持有 fd 关心事件类型
    int revents_;     // it's the received event types of epoll or poll，目前活动的事件类型，由 Poll 修改返回
    int index_;       // used by Poller. 在 Poll 中进行使用
    bool logHup_;
    // 这个 tie 用来保存 TcpConnection 对象指针，没来锁定
    std::weak_ptr<void> tie_;
    // 标识是否关联了 TcpConnection
    bool tied_;
    bool eventHandling_; // 用来表示正在处理事件，保证不会在析构处进行删除
    bool addedToLoop_;   // 用来标识是否在 poll 中
    // 总共只有 4 种事件的对应处理函数
    ReadEventCallback readCallback_; // 可读
    EventCallback writeCallback_;    // 可写
    EventCallback closeCallback_;    // 关闭
    EventCallback errorCallback_;    // 出错
};

} // namespace net
} // namespace muduo

#endif // MUDUO_NET_CHANNEL_H
