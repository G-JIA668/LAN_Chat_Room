# LAN_Chat_Room
基于muduo库的局域网聊天室

项目背景：
刚学完muduo网络库，准备实践一下，所以选择这个项目。
在这个项目中，有一个聊天记录服务器，需要提前启动。
每个新加入局域网聊天室的主机，在登录前可以获取近几天的聊天记录。
传递聊天记录使用tcp协议，除此之外，聊天室成员间传递消息均使用udp协议。
使用tcp协议是为了复用muduo库的代码用例，
使用udp协议是是因为局域网网络环境简单，很少有丢包问题。
在这个项目中并没有使用Qt来显示输入输出框，而是使用了exec,mkfifo等系统调用，以及gnome-terminal程序作为输入框。

运行环境：
Ubuntu 20.04
boost库      //未安装则使用 "sudo apt-get install libboost-dev libboost-test-dev" 安装
muduo库      //头文件在该项目的include文件夹中，静态库文件在该项目的lib文件夹中，无需安装

运行步骤：
1.在A、B、C主机运行：
./build.sh  //需要用到CMake，未安装则使用 "sudo apt-get install cmake" 安装
2.在A主机运行
./build/bin/chatlogserver.exe       //作为聊天记录服务器
3.在B主机运行
./build/bin/joiner.exe <your name>  //作为参与者
4.在C主机运行
./build/bin/joiner.exe <your name>  //作为参与者
（如果只有一台主机，也可以运行 聊天记录服务器程序 和一个 参与者 程序，
因为聊天服务器的tcp sockfd和udp sockfd都使用了1900端口，
而 参与者 程序的udp sockfd如果拿不到1900端口，系统会给它分配其他可用端口。）
