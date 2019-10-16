// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include "muduo/base/noncopyable.h"
#include "muduo/base/StringPiece.h"
#include "muduo/base/Types.h"
#include "muduo/net/Callbacks.h"
#include "muduo/net/Buffer.h"
#include "muduo/net/InetAddress.h"

#include <memory>

#include <boost/any.hpp>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
// 最基础的连接类，尽量少的暴露细节
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
public:
    /// Constructs a TcpConnection with a connected sockfd
    ///
    /// User should not create this object.
    // 用户不可以创建这个类，暂时看来，仅仅 TcpServer 会创建这个类
    TcpConnection(EventLoop *loop,
                  const string &name,
                  int sockfd,
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);
    ~TcpConnection();

    // 一系列基本操作
    EventLoop *getLoop() const { return loop_; }
    const string &name() const { return name_; }
    const InetAddress &localAddress() const { return localAddr_; }
    const InetAddress &peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }
    // return true if success.
    bool getTcpInfo(struct tcp_info *) const;
    string getTcpInfoString() const;

    // void send(string&& message); // C++11
    void send(const void *message, int len);
    void send(const StringPiece &message);
    // void send(Buffer&& message); // C++11
    void send(Buffer *message); // this one will swap data
    void shutdown();            // NOT thread safe, no simultaneous calling
    // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
    void forceClose();
    void forceCloseWithDelay(double seconds);
    void setTcpNoDelay(bool on);
    // reading or not
    void startRead();
    void stopRead();
    bool isReading() const { return reading_; }; // NOT thread safe, may race with start/stopReadInLoop

    void setContext(const boost::any &context)
    {
        context_ = context;
    }

    const boost::any &getContext() const
    {
        return context_;
    }

    boost::any *getMutableContext()
    {
        return &context_;
    }

    void setConnectionCallback(const ConnectionCallback &cb)
    {
        connectionCallback_ = cb;
    }

    void setMessageCallback(const MessageCallback &cb)
    {
        messageCallback_ = cb;
    }

    void setWriteCompleteCallback(const WriteCompleteCallback &cb)
    {
        writeCompleteCallback_ = cb;
    }

    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark)
    {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

    /// Advanced interface
    Buffer *inputBuffer()
    {
        return &inputBuffer_;
    }

    Buffer *outputBuffer()
    {
        return &outputBuffer_;
    }

    /// Internal use only.
    void setCloseCallback(const CloseCallback &cb)
    {
        closeCallback_ = cb;
    }

    // called when TcpServer accepts a new connection
    void connectEstablished(); // should be called only once
    // called when TcpServer has removed me from its map
    void connectDestroyed(); // should be called only once

private:
    enum StateE
    {
        kDisconnected, // 断开连接
        kConnecting,   // 正在连接
        kConnected,    // 已连接
        kDisconnecting // 正在断开连接
    };
    // 各种处理操作
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    // void sendInLoop(string&& message);
    void sendInLoop(const StringPiece &message);
    void sendInLoop(const void *message, size_t len);
    void shutdownInLoop();
    // void shutdownAndForceCloseInLoop(double seconds);
    void forceCloseInLoop();
    void setState(StateE s) { state_ = s; }
    const char *stateToString() const;
    void startReadInLoop();
    void stopReadInLoop();

    EventLoop *loop_;    // 对应的 EventLoop
    const string name_;  // name_，用来在 TcpServer 中进行区分
    StateE state_; // FIXME: use atomic variable 状态
    bool reading_; // 是否正在执行读操作
    // we don't expose those classes to client.
    // 用 unique_ptr 进行保存的一组 socket 和 channel
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    // 本段和对端地址
    const InetAddress localAddr_;
    const InetAddress peerAddr_;
    // 五种回调函数
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    CloseCallback closeCallback_;
    size_t highWaterMark_;  // 高水位标识
    // 输入输出缓冲区
    Buffer inputBuffer_;
    Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer.
    boost::any context_;
    // FIXME: creationTime_, lastReceiveTime_
    //        bytesReceived_, bytesSent_
};
// 存储在 TcpServer 中的指针
typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

} // namespace net
} // namespace muduo

#endif // MUDUO_NET_TCPCONNECTION_H
