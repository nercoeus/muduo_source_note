#ifndef MUDUO_EXAMPLES_SIMPLE_ECHO_ECHO_H
#define MUDUO_EXAMPLES_SIMPLE_ECHO_ECHO_H

#include "muduo/net/TcpServer.h"

// RFC 862
class EchoServer
{
public:
  EchoServer(muduo::net::EventLoop *loop,
             const muduo::net::InetAddress &listenAddr);
  // 服务器开始运行得接口
  void start(); // calls server_.start();

private:
  void onConnection(const muduo::net::TcpConnectionPtr &conn);

  void onMessage(const muduo::net::TcpConnectionPtr &conn,
                 muduo::net::Buffer *buf,
                 muduo::Timestamp time);
  void onWriteComplete(const muduo::net::TcpConnectionPtr& conn);
  // 核心服务器
  muduo::net::TcpServer server_;
  muduo::string message_;
};

#endif // MUDUO_EXAMPLES_SIMPLE_ECHO_ECHO_H
