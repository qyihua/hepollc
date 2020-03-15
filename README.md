# hepollc
基于epoll的协程框架，包含一个http服务器，本框架是单进程单线程模型，利用汇编实现协程的上下文切换，本项目代码仅供参考学习，不可用于实际生产环境

# 编译要求64位Linux操作系统，gcc7及更新版本编译器，OpenSSL 1.1.0以上版本

# Hello World
参考 src/app/helloworld/ 目录

# 编译
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
或
cmake .. -DCMAKE_BUILD_TYPE=Release
make
cd ../bin
./hepollc

即可使用浏览器打开 Hello World
http://127.0.0.1:8088/roothello
http://127.0.0.1:8088/hello/apphello


# 数据库支持
默认禁用了数据库支持，可通过编辑 src/CMakeLists.txt 去掉 #add_subdirectory(dblib) 注释开启
目前只实现了 Oracle，Postgesql 的支持，其他数据库可参考 src/dblib

# 配置
可通过命令行进行配置，例如 ./hepollc -port 8088 -process_num 4 -reuseport 1
也可通过 bin/conf/ 目录进行配置
