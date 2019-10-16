// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/Acceptor.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/SocketsOps.h"

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;
// 构造函数，要传入一个地址（IP+Port）
Acceptor::Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport)
    : loop_(loop),   // 一般情况下是主循环
      // 调用 socket 传入协议族 创建一个 noblock 的套接字
      acceptSocket_(sockets::createNonblockingOrDie(listenAddr.family())), 
      acceptChannel_(loop, acceptSocket_.fd()),  // channel 是对 fd 的封装
      listenning_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC))
{
    assert(idleFd_ >= 0);
    // 开启 SO_REUSEADDR 套接字选项，可以重复连接
    acceptSocket_.setReuseAddr(true);
    // 设置可以重复连接的 port
    acceptSocket_.setReusePort(reuseport);
    // 调用 bind
    acceptSocket_.bindAddress(listenAddr);
    // 可读的话执行 Acceptor::handleRead，监听套接字，只注册可读操作就可以了
    acceptChannel_.setReadCallback(
        std::bind(&Acceptor::handleRead, this));
}

Acceptor::~Acceptor()
{
    // 关闭套接字描述符的所有关心事件
    acceptChannel_.disableAll();
    // 把这个 channel 从 loop 中删除
    acceptChannel_.remove();
    // 关闭 idleFd_ 套接字描述符，直接 close 连包装函数都不用
    ::close(idleFd_);
}
// 调用 listen
void Acceptor::listen()
{
    // 必须在自己所处的线程中才可以调用
    loop_->assertInLoopThread();
    // 开启监听标志
    listenning_ = true;
    // 调用 listen，listen 接口通过 socket 提供
    acceptSocket_.listen();
    // 注册 read 事件，注意这个 channel 的 read 回调函数在初始化的时候就注册好了
    acceptChannel_.enableReading();
}

// 可读连接，新的连接，当 channel 可读时，执行这个函数
void Acceptor::handleRead()
{
    loop_->assertInLoopThread();
    InetAddress peerAddr;
    //FIXME loop until no more
    // 调用 accept，获得对端的 inetAddr 和创建一个 fd
    int connfd = acceptSocket_.accept(&peerAddr);
    if (connfd >= 0)
    {
        // string hostport = peerAddr.toIpPort();
        // LOG_TRACE << "Accepts of " << hostport;
        if (newConnectionCallback_)
        {
            // 回调 TcpServer::newConnect 即可
            newConnectionCallback_(connfd, peerAddr);
        }
        else
        {
            // 关闭套接字
            sockets::close(connfd);
        }
    }
    else
    {
        // 出错处理
        LOG_SYSERR << "in Acceptor::handleRead";
        // Read the section named "The special problem of
        // accept()ing when you can't" in libev's doc.
        // By Marc Lehmann, author of libev.
        // 看 libev 文档: 无法接收连接时的特殊问题
        // 原因大致如下：
        // 描述符用完的情况下，会导致 accept() 失败，返回 ENFILE 错误，但并没有拒绝这个连接
        // 它仍在队列里等待连接，这导致在下一次迭代的时候，仍然会触发监听描述符的可读事件，这导致程序busy loop
        // 这里就借用 idleFd_ 保存的一个描述符对其进行 accept 并关闭，然后再重置 idleFd_ 即可
        if (errno == EMFILE)
        {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
