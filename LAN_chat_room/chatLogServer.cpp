#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <string.h>
#include <cstring>
#include <dirent.h>

#include <muduo/base/LogFile.h>
#include <muduo/base/Logging.h>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/FileUtil.h>
#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

class Receiver
{
public:
    Receiver(uint16_t sockNum) : sockNum_(sockNum)
    {
        socketInit();
        logFileInit();
    }

    void run()
    {
        struct sockaddr_in cliAddr;
        socklen_t cliAddrLen = sizeof(struct sockaddr_in);
        size_t len;
        while (1)
        {
            bzero(buff_, sizeof(buff_));
            len = recvfrom(sockfd_, buff_, sizeof(buff_), 0, reinterpret_cast<struct sockaddr *>(&cliAddr), &cliAddrLen);
            printf("recv udp from %s : %s\n", inet_ntoa(cliAddr.sin_addr), buff_);
            g_logFile->append(buff_, static_cast<int>(len));
            g_logFile->flush();
        }
    }

private:
    void socketInit()
    {
        //绑定udp的sockfd与本机地址,端口号
        sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
        bzero(&myAddr_, sizeof(myAddr_));
        myAddr_.sin_family = AF_INET;
        myAddr_.sin_addr.s_addr = htons(INADDR_ANY);
        myAddr_.sin_port = htons(sockNum_);
        int ret;
        if (ret = bind(sockfd_, reinterpret_cast<struct sockaddr *>(&myAddr_), sizeof(myAddr_)) < 0)
        {
            perror("bind failed\n");
        }
    }

    void logFileInit()
    {
        g_logFile.reset(new muduo::LogFile("charLog", 200 * 1000));
    }
    void rollFile()
    {
        g_logFile->rollFile();
    }

private:
    int sockfd_;
    uint16_t sockNum_;
    struct sockaddr_in myAddr_;
    static boost::scoped_ptr<muduo::LogFile> g_logFile;
    char buff_[256];
};
boost::scoped_ptr<muduo::LogFile> Receiver::g_logFile;

class Distributor
{
private:
    class SearchFile
    {
    public:
        static int getTime()
        {
            char timebuf[32];
            struct tm tm;
            time_t now= time(NULL);
            localtime_r(&now, &tm); // FIXME: localtime_r ?
            strftime(timebuf, sizeof timebuf, "%Y%m%d", &tm);
            return atoi(timebuf);
        }

        static void getLogs(int Ndays, string *result)
        {
            /*ordered by dirent.d_off,which is useless at all
            DIR *dir = opendir("./LOGFILES/");
            struct dirent *d;
            struct stat statbuf;
            string temp;
            while (d = readdir(dir))
            {
                stat(d->d_name, &statbuf);
                if (S_ISDIR(statbuf.st_mode))
                {
                    if (strcmp(d->d_name, ".") == 0)
                        continue;
                    if (strcmp(d->d_name, "..") == 0)
                        continue;
                }
                printf("name:%s\n", d->d_name);
                printf("size:%ld\n", statbuf.st_size);

                int64_t size = 0;
                string fname = "./LOGFILES/" + string(d->d_name);
                FileUtil::readFile(fname.c_str(), 1024, &temp, &size);
                *result += temp;
            }
            std::cout << "result:" << *result << std::endl;
            */
            // ordered by filename using alphasort()
            struct dirent **namelist;
            struct dirent *d;
            struct stat statbuf;
            string temp;
            char datebuf[10];
            int n;
            n = scandir("./LOGFILES/", &namelist, 0, alphasort);
            if (n < 0)
            {
                printf("scandir return %d\n", n);
            }
            else
            {
                int index = 0;
                while (index < n)
                {
                    d = namelist[index];
                    stat(d->d_name, &statbuf);
                    if (S_ISDIR(statbuf.st_mode))
                    {
                        if (strcmp(d->d_name, ".") == 0)
                        {
                            index++;
                            continue;
                        }
                        else if (strcmp(d->d_name, "..") == 0)
                        {
                            index++;
                            continue;
                        }
                    }
                   // printf("name:%s\n", d->d_name);
                   // printf("size:%ld\n", statbuf.st_size);
                    strncpy(datebuf, d->d_name + 8, 8);
                    int oldDate = atoi(datebuf);
                   // printf("date:%d\n", oldDate);
                    int today = getTime();
                   // printf("today:%d\n",today);
                    if (today - Ndays <= oldDate)
                    {
                        int64_t size = 0;
                        string fname = "./LOGFILES/" + string(d->d_name);
                        FileUtil::readFile(fname.c_str(), 1024, &temp, &size);
                        *result += temp;
                    }
                    free(d);
                    index++;
                }
                free(namelist);
            }
            //std::cout << "result:" << *result << std::endl;
        }
    };

    void flushLogFile()
    {
        g_logFile->rollFile();
        printf("flushLogFile\n");
    }
    void rollEvery(int time)
    {
        flushLogFile();
        loop_->runEvery(time, boost::bind(&Distributor::flushLogFile, this));
    }

    int nextTime()
    {
        time_t now0 = 0;
        now0 = time(NULL);
        struct tm tm0, tm8;
        gmtime_r(&now0, &tm0);
        localtime_r(&now0, &tm8);
        int zone = (24 + tm8.tm_hour - tm0.tm_hour) % 24;
        long int now8 = now0 + zone * 60 * 60;
        int secleft = 24 * 60 * 60 - static_cast<int>(now8 % (24 * 60 * 60));
        return secleft;
    }

public:
    Distributor(EventLoop *loop,
                const InetAddress &listenAddr, const string &name)
        : loop_(loop),
          server_(loop, listenAddr, name)
    {
        server_.setConnectionCallback(
            boost::bind(&Distributor::onConnection, this, _1));
        server_.setMessageCallback(
            boost::bind(&Distributor::onMessage, this, _1, _2, _3));
        server_.setWriteCompleteCallback(
            boost::bind(&Distributor::onWriteComplete, this, _1));

        loop->runAfter(1, boost::bind(&Distributor::start, this));

        loop->runAfter(nextTime(), boost::bind(&Distributor::rollEvery, this, 24 * 60 * 60));
    }

    void start()
    {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            printf("onConnection(): new connection [%s] from %s\n",
                   conn->name().c_str(),
                   conn->peerAddress().toIpPort().c_str());

            conn->setTcpNoDelay(true);
           // conn->send("now send you chatlog\n");
        }
        else
        {
            printf("onConnection(): connection [%s] is down\n",
                   conn->name().c_str());
        }
    }

    void onMessage(const TcpConnectionPtr &conn,
                   Buffer *buf,
                   Timestamp receiveTime)
    {
        if (buf->readableBytes() > 14)
        {
            muduo::string header = buf->retrieveAsString(15 - 1);
           // std::cout << "header=" << header << std::endl;
           // std::cout << "size=" << header.size() << std::endl;
            if (header.compare("CHAT_LOG_FILES") == 0)
            {
                string result;
                int64_t size = 0;
                muduo::string Ndays(buf->retrieveAllAsString());
                SearchFile::getLogs(atoi(Ndays.c_str()), &result);
                // std::cout << "send result" << result << std::endl;
                conn->send(result);
            }
        }
        server_.removeConnection(conn);
    }

    void onWriteComplete(const TcpConnectionPtr &conn)
    {
        conn->send(message_);
    }

    EventLoop *loop_;
    TcpServer server_;
    muduo::string message_;
    static boost::scoped_ptr<muduo::LogFile> g_logFile;
};
boost::scoped_ptr<muduo::LogFile> Distributor::g_logFile;

int main(int argc, char **argv)
{
    Logger::setLogLevel(Logger::WARN);
    printf("Running...\n");
    EventLoopThread t; // 独立线程分发聊天记录
    Distributor distributor(t.startLoop(), InetAddress(1900), "distirbutor");

    Receiver receiver(1900);
    receiver.run(); //主线程接收聊天记录

    return 0;
}
//——————————————————————————————
