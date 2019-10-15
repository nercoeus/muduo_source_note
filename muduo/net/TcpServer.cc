// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/TcpServer.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Acceptor.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/SocketsOps.h"

#include <stdio.h> // snprintf

using namespace muduo;
using namespace muduo::net;
// 初始化一个 server，传入:主进程中初始化的 loop，和 inetaddress 对象
TcpServer::TcpServer(EventLoop *loop,
                     const InetAddress &listenAddr,
                     const string &nameArg,
                     Option option)
    : loop_(CHECK_NOTNULL(loop)),     // 主进程的 Loop
      ipPort_(listenAddr.toIpPort()), // 创建的 Loop 中的 port
      name_(nameArg),   // Server 的名字
      acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)), // 在 acceptor 初始划时，就已经进行了 shock(),bind(),但还没有执行 listen()
      threadPool_(new EventLoopThreadPool(loop, name_)),  // 创建 ThreadPool 但是默认初始化数量为 0
      connectionCallback_(defaultConnectionCallback),     // 默认的建立链接后的函数
      messageCallback_(defaultMessageCallback),           // 默认的消息处理函数
      nextConnId_(1)    // conn 的 ID 从 1 开始累加
{
    // 使用 acceptor 调用 newConnection，初始化时注册回调，在 acceptor 中的连接建立回调函数调用 TcpServer::newConnection
    acceptor_->setNewConnectionCallback(
        std::bind(&TcpServer::newConnection, this, _1, _2));    // 使用 bind 函数来进行绑定，注意这里传入了 this 指针
}

// 服务器析构函数
TcpServer::~TcpServer()
{
    loop_->assertInLoopThread();
    LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";
    // 对所有链接需要进行断开操作
    for (auto &item : connections_)
    {
        // 获取链接指针，即 shared_ptr<TcpConnection>
        TcpConnectionPtr conn(item.second);
        // 删除这个指针
        item.second.reset();
        // 在这个链接对应的线程中进行操作
        conn->getLoop()->runInLoop(
            std::bind(&TcpConnection::connectDestroyed, conn)); // 调用的是 TcpConnection 的方法，传入 conn
    }
}

// 设置 ThreadPool 的大小，这里调用的是 setThreadNum
void TcpServer::setThreadNum(int numThreads)
{
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}
// 服务器开始运行的接口，这里仅仅完成服务器加载操作，执行完会执行 loop() 才会真正的跑起来
void TcpServer::start()
{
    // 判断是否已经开始运行，没有开始运行才会执行下面的步骤
    if (started_.getAndSet(1) == 0)
    {
        // 初始化线程池,并传入回调函数，根据 ThreadPoolNum 来初始化需要个数的线程
        threadPool_->start(threadInitCallback_);   // 注册 threadInitCallback_ 函数，可用可不用
        // 判断 acceptor 对象的监听状态，此时应该没有监听
        assert(!acceptor_->listenning());
        // 在 loop 中调用 Acceptor::listen 函数
        loop_->runInLoop(
            std::bind(&Acceptor::listen, get_pointer(acceptor_)));
    }
}

// 建立新连接，主服务器只处理连接的建立和断开，通过将 newConnection 注册给 acceptor 的方式来使其在新链接建立的时候由 TcpServer 进行调用
void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr)
{
    loop_->assertInLoopThread();
    // 获取一个 EventLoop 对象
    EventLoop *ioLoop = threadPool_->getNextLoop();
    char buf[64];
    snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    // 根据 name_ 和 ConnId 来获取不同的名字
    string connName = name_ + buf;

    LOG_INFO << "TcpServer::newConnection [" << name_
             << "] - new connection [" << connName
             << "] from " << peerAddr.toIpPort();
    // 获取本地的 localAddr
    InetAddress localAddr(sockets::getLocalAddr(sockfd));
    // FIXME poll with zero timeout to double confirm the new connection
    // FIXME use make_shared if necessary
    // 创建一个 TcpConnection 对象用来获取 fd，创建 TcpConnection 传入获取的 EventLoop、connName、fd、localAddr、peerAddr 即可
    TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                            connName,
                                            sockfd,
                                            localAddr,
                                            peerAddr));
    // 创建好的 TcpConnection 添加到 TcpServer 管理下
    connections_[connName] = conn;
    // 对处理函数进行一系列注册，包括连接和读、写，这些注册函数保存在 conn 中
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // 注册断开连接时的操作
    conn->setCloseCallback(
        std::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe
    // 执行 connectEstablished 函数把 conn 添加到 eventPool 中的 poll 中
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

// 断开连接时的操作，暂时不明白包装一次是为了啥，第一次可能是不在当前线程中（毕竟是 conn 对应的线程进行操作）
// 将 removeConnectionInLoop 添加到主线程中进行执行
void TcpServer::removeConnection(const TcpConnectionPtr &conn)
{
    // FIXME: unsafe
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

// 断开链接的操作
void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn)
{
    loop_->assertInLoopThread();
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
             << "] - connection " << conn->name();
    // 删除 TcpConnection 数组中的数据
    size_t n = connections_.erase(conn->name());
    // tips: (void)n 是为了防止编辑器 warning 这个 n 没有使用
    (void)n;
    assert(n == 1);
    // 在对应的 EventLoopThread 的 Poll 中删除 TcpConnection 关注的操作
    EventLoop *ioLoop = conn->getLoop();
    ioLoop->queueInLoop(
        std::bind(&TcpConnection::connectDestroyed, conn));  // 调用 TcpConnection::connectDestroyed
}
