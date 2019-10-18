#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/base/Logging.h"
#include <stdio.h>
#include <unistd.h>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

// TestServer中包含一个TcpServer
class TestServer
{
public:
  TestServer(muduo::net::EventLoop *loop,
             const muduo::net::InetAddress &listenAddr)
      : server_(loop, listenAddr, "TestServer")
  {
    // 设置连接建立/断开、消息到来、消息发送完毕回调函数
    server_.setConnectionCallback(
        std::bind(&TestServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&TestServer::onMessage, this, _1, _2, _3));
    server_.setWriteCompleteCallback(
        std::bind(&TestServer::onWriteComplete, this, _1));

    // 生成数据
    muduo::string line;
    for (int i = 33; i < 127; ++i)
    {
      line.push_back(char(i));
    }
    line += line;

    for (size_t i = 0; i < 127 - 33; ++i)
    {
      message_ += line.substr(i, 72) + '\n';
    }
  }

  void start()
  {
    server_.start();
  }

private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn)
  {
    if (conn->connected())
    {
      printf("onConnection(): new connection [%s] from %s\n",
             conn->name().c_str(),
             conn->peerAddress().toIpPort().c_str());

      conn->setTcpNoDelay(true);
      conn->send(message_);
    }
    else
    {
      printf("onConnection(): connection [%s] is down\n",
             conn->name().c_str());
    }
  }

  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer *buf,
                 muduo::Timestamp receiveTime)
  {
    muduo::string msg(buf->retrieveAllAsString());
    printf("onMessage(): received %zd bytes from connection [%s] at %s\n",
           msg.size(),
           conn->name().c_str(),
           receiveTime.toFormattedString().c_str());

    conn->send(msg);
  }

  // 消息发送完毕回调函数，继续发送数据
  void onWriteComplete(const muduo::net::TcpConnectionPtr& conn)
  {
    conn->send(message_);
  }

  muduo::net::TcpServer server_;

  muduo::string message_;
};

int main()
{
  printf("main(): pid = %d\n", getpid());

  muduo::net::InetAddress listenAddr(8888);
  muduo::net::EventLoop loop;

  TestServer server(&loop, listenAddr);
  server.start();

  loop.loop();
}