#include <muduo/net/Channel.h>
#include <muduo/net/TcpClient.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <boost/bind.hpp>
#include <iostream>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <algorithm>

#include <unistd.h>
#include <stdlib.h>    //exit(0)
#include <stdio.h>     //fgets
#include <cstring>     //strlen
#include <sys/types.h> //waitpid
#include <sys/stat.h>  //mkfifo
#include <sys/wait.h>  //waitpid
#include <fcntl.h>     //O_RDONLY
#include <errno.h>     //errno
#include "include/fifo.h"

using namespace muduo;
using namespace muduo::net;

class Joiner
{
public:
  Joiner(EventLoop *loop, const InetAddress &listenAddr, int forNdays, char *myname, int readfd, int sockfd)
      : loop_(loop),
        client_(loop, listenAddr, "Joiner"),
        stdinChannel_(loop, readfd),
        othChannel_(loop, sockfd),
        forNdays_(forNdays),
        readfd_(readfd),
        sockfd_(sockfd)
  {
    client_.setConnectionCallback(
        boost::bind(&Joiner::onConnection, this, _1));
    client_.setMessageCallback(
        boost::bind(&Joiner::onMessage, this, _1, _2, _3));
    // client_.enableRetry();
    //  标准输入缓冲区中有数据的时候，回调TestClient::handleRead
    stdinChannel_.setReadCallback(boost::bind(&Joiner::handleRead, this));
    stdinChannel_.setCloseCallback(boost::bind(&Joiner::handleClose, this));
    othChannel_.setReadCallback(boost::bind(&Joiner::handleRead2, this));
    stdinChannel_.enableReading(); // 关注可读事件
    othChannel_.enableReading();
    // std::cout<<"get the name:"<<myname;
    memset(myname_, ' ', sizeof(myname_));
    int fitLen = static_cast<int>(std::min(strlen(myname), sizeof(myname_) - 1));
    memcpy(myname_, myname, fitLen);
    myname_[sizeof(myname_) - 5] = '\n';
    // std::cout<<"now name is"<<myname_;
    udpInit();
  }

  void connect()
  {
    client_.connect();
  }

private:
  void onConnection(const TcpConnectionPtr &conn)
  {
    if (conn->connected())
    {
      // printf("onConnection(): new connection [%s] from %s\n",
      //        conn->name().c_str(),
      //        conn->peerAddress().toIpPort().c_str());
      printf("[asking for chat record ...]\n");
      std::string s = "CHAT_LOG_FILES" + std::to_string(forNdays_);
      // std::cout << s << std::endl;
      // std::cout << s.size() << std::endl;
      client_.connection()->send(s);
    }
    else
    {
      // printf("onConnection(): connection [%s] is down\n",
      //       conn->name().c_str());
      printf("[completed]\n");
    }
  }

  void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp time)
  {
    string msg(buf->retrieveAllAsString());
    printf("%s\n", msg.c_str());
    // LOG_TRACE << conn->name() << " recv " << msg.size() << " bytes at " << time.toFormattedString();
  }

  // 标准输入缓冲区中有数据的时候，回调该函数
  void handleRead()
  {
    int namelen = sizeof(myname_);
    int buflen = sizeof(sendbuf);
    int space = buflen - namelen;
    memcpy(sendbuf, myname_, buflen);
    bzero(sendbuf + namelen, space);
    // std::cin.getline(sendbuf + namelen, space - 1);
    ssize_t n;
    if ((n = read(readfd_, sendbuf + namelen, MAXLINE)) == 0)
      perror("end-of-file while reading pathname");
    char *p = sendbuf;
    unsigned long messageLen = strlen(p);
    sendbuf[messageLen] = '\n';

    if ((n = sendto(sockfd_, sendbuf, messageLen + 1, 0, reinterpret_cast<struct sockaddr *>(&servaddr), sizeof(struct sockaddr))) < 0)
    {
      perror("sendto failed");
    }
    printf("%s", sendbuf);
  }

  void handleRead2()
  {
    bzero(&tempaddr, sizeof(tempaddr));
    recvfrom(sockfd_, revbuf, sizeof(revbuf), 0, reinterpret_cast<struct sockaddr *>(&tempaddr), &tempaddrlen);
    printf("%s", revbuf);
  }

  void handleClose()
  {
    loop_->quit();
    printf("NO INPUT BOX!\n");
  }

  void udpInit()
  {
    bzero(&servaddr, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(1900);
    if (inet_pton(AF_INET, "255.255.255.255", &servaddr.sin_addr) < 0)
    {
      perror("inet_pton error");
    }

    bzero(&cliaddr, sizeof(struct sockaddr_in));
    cliaddr.sin_family = AF_INET;
    cliaddr.sin_port = htons(1900);
    cliaddr.sin_addr.s_addr = htons(INADDR_ANY);
    /*if (inet_pton(AF_INET, "127.0.0.1", &cliaddr.sin_addr) < 0)
    {
      perror("inet_pton error for cliaddr\n");
    }*/

    if ((n = bind(sockfd_, reinterpret_cast<struct sockaddr *>(&cliaddr), sizeof(struct sockaddr))) < 0)
    {
      perror("bind failed");
    }

    opt = 1;
    if ((n = setsockopt(sockfd_, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt))) < 0)
    {
      perror("sesockopt failed");
    }
  }

  int sockfd_, n, opt;
  char sendbuf[256], revbuf[256];
  struct sockaddr_in servaddr, cliaddr;
  struct sockaddr_in tempaddr;
  socklen_t tempaddrlen = sizeof(struct sockaddr_in);

  int forNdays_;
  char myname_[20];
  int readfd_;
  EventLoop *loop_;
  TcpClient client_;
  Channel stdinChannel_; // 标准输入Channel
  Channel othChannel_;
};

int main(int argc, char *argv[])
{
  while (argc != 2)
  {
    printf("usage: %s <your name>\n", argv[0]);
    return 0;
  }
  int forNdays = 0;
  do
  {
    std::cout << "get chatlog in past N days (N=0,1,2...)" << std::endl;
    std::cin >> forNdays;
  } while (forNdays < 0);
  std::cout << "=============================================" << std::endl;

  int pid;

  if ((pid = fork()) == -1)
  {
    perror("fork");
  }

  if (pid == 0)
  {
    close(2);
    execlp("gnome-terminal", "gnome-terminal","--window","--title=Input Box","--geometry=70x12+600+500","-x", "bash", "-c", "./inputbox", NULL);
  }
  if (pid != 0)
  {
    wait(NULL);

    if ((mkfifo(FIFO1, FILE_MODE) < 0) && (errno != EEXIST))
    {
      printf("can't create %s\n", FIFO1);
    }

    int readfd;
    if ((readfd = open(FIFO1, O_RDONLY, 0)) < 0)
    {
      perror("FIFO error\n");
    }
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
      perror("socket error\n");
    }

    Logger::setLogLevel(Logger::WARN);
    EventLoop loop;
    InetAddress serverAddr("127.0.0.1", 1900);
    Joiner joiner(&loop, serverAddr, forNdays, argv[1], readfd, sockfd);
    joiner.connect();
    loop.loop();
  }
}
