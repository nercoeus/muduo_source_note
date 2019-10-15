// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <functional>

#include "muduo/net/Channel.h"
#include "muduo/net/Socket.h"

namespace muduo
{
namespace net
{

class EventLoop;
class InetAddress;

///
/// Acceptor of incoming TCP connections.
///
// 接收新的连接,用来接受一个新的连接
class Acceptor : noncopyable
{
public:
    typedef std::function<void(int sockfd, const InetAddress &)> NewConnectionCallback;
    // 构造函数，析构函数
    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();
    // 设置新连接回调函数
    void setNewConnectionCallback(const NewConnectionCallback &cb)
    {
        newConnectionCallback_ = cb;
    }
    // 返回监听状态
    bool listenning() const { return listenning_; }
    // 开始监听，对 listen 的封装
    void listen();

private:
    // 处理套接字可读，在 accept 中，就是新连接
    void handleRead();
    // 所在的事件循环（一般为主循环）
    EventLoop *loop_;
    // 对 fd 的封装（监听套接字）
    Socket acceptSocket_;
    // 监听 channel（监听套接字对应的 channel）
    Channel acceptChannel_;
    // 建立新连接后的回调函数，这个函数在 accept 所拥有的对象中进行注册
    // 并不是 accept 创建时，也就是在 TcpServer 中进行注册
    // 并且注册的函数也定义在 TcpServer 中
    NewConnectionCallback newConnectionCallback_;
    // 是否正在监听的标志
    bool listenning_;
    int idleFd_;
};

} // namespace net
} // namespace muduo

#endif // MUDUO_NET_ACCEPTOR_H
