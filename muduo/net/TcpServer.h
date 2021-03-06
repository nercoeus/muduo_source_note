// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPSERVER_H
#define MUDUO_NET_TCPSERVER_H

#include "muduo/base/Atomic.h"
#include "muduo/base/Types.h"
#include "muduo/net/TcpConnection.h"

#include <map>

namespace muduo
{
namespace net
{

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

///
/// TCP server, supports single-threaded and thread-pool models.
///
/// This is an interface class, so don't expose too much details.
// server class,use single-thread and threadpool models
// 对应的服务器类
class TcpServer : noncopyable
{
public:
    typedef std::function<void(EventLoop *)> ThreadInitCallback;
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    //TcpServer(EventLoop* loop, const InetAddress& listenAddr);
    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const string &nameArg,
              Option option = kNoReusePort);
    ~TcpServer(); // force out-line dtor, for std::unique_ptr members.

    const string &ipPort() const { return ipPort_; }
    const string &name() const { return name_; }
    EventLoop *getLoop() const { return loop_; }

    /// Set the number of threads for handling input.
    ///
    /// Always accepts new connection in loop's thread.
    /// Must be called before @c start
    /// @param numThreads
    /// - 0 means all I/O in loop's thread, no thread will created.
    ///   this is the default value.
    /// - 1 means all I/O in another thread.
    /// - N means a thread pool with N threads, new connections
    ///   are assigned on a round-robin basis.
    // 设置线程池大小
    void setThreadNum(int numThreads);
    void setThreadInitCallback(const ThreadInitCallback &cb)
    {
        threadInitCallback_ = cb;
    }
    /// valid after calling start()
    // 返回线程池
    std::shared_ptr<EventLoopThreadPool> threadPool()
    {
        return threadPool_;
    }

    /// Starts the server if it's not listenning.
    ///
    /// It's harmless to call it multiple times.
    /// Thread safe.
    // 没有运行的话启动 server
    void start();

    /// Set connection callback.
    /// Not thread safe.
    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }

    /// Set message callback.
    /// Not thread safe.
    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }

    /// Set write complete callback.
    /// Not thread safe.
    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }

private:
    /// Not thread safe, but in loop
    /// 不是线程安全的,但是在单个线程中进行执行，因为每个 acceptor 均占用一个 eventLoop
    void newConnection(int sockfd, const InetAddress &peerAddr);
    /// Thread safe.线程安全操作
    void removeConnection(const TcpConnectionPtr &conn);
    /// Not thread safe, but in loop
    void removeConnectionInLoop(const TcpConnectionPtr &conn);
    // safe all Tcp connect,use string as key
    // map[连接名字]连接对象
    typedef std::map<string, TcpConnectionPtr> ConnectionMap;
    // 主循环的 EventLoop，一般仅仅执行 newConnect 操作
    // 该 loop_ 由客户进行创建，并不是在 TcpServer 中自动进行创建，这是因为会有多个监听接口，但是同一个线程只能有一个 Eventloop
    EventLoop *loop_;     // the acceptor loop
    const string ipPort_; // port
    const string name_;   // 服务器 name
    // 用来接受新的连接的 acceptor，使用 unique_ptr 包装指针
    std::unique_ptr<Acceptor> acceptor_; // avoid revealing Acceptor
    // 线程池，使用 shared_ptr 包装
    std::shared_ptr<EventLoopThreadPool> threadPool_;
    // 注册的回调函数，由用户进行注册
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;
    // 保持原子操作，用来记录服务器是否正在 loop
    AtomicInt32 started_;
    // always in loop thread
    // 不用设置为 AtomicInt32 因为仅仅在本线程中进行调用
    int nextConnId_;
    // 用来保存所有连接对象
    ConnectionMap connections_;
};

} // namespace net
} // namespace muduo

#endif // MUDUO_NET_TCPSERVER_H
