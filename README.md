# hepollc
基于epoll的协程框架，包含一个http服务器，本框架是单进程单线程模型，利用汇编实现协程的上下文切换，本项目代码仅供参考学习，不可用于实际生产环境

# 编译要求64位Linux操作系统，gcc7及更新版本编译器，OpenSSL 1.1.0以上版本

# Hello World
参考 src/app/helloworld/ 目录

# 编译
mkdir build <br>
cd build<br>
cmake .. -DCMAKE_BUILD_TYPE=Debug<br>
或<br>
cmake .. -DCMAKE_BUILD_TYPE=Release<br>
make<br>
cd ../bin<br>
./hepollc<br>
<br><br>
即可使用浏览器打开 Hello World<br>
http://127.0.0.1:8088/roothello<br>
http://127.0.0.1:8088/hello/apphello<br>


# 数据库支持
默认禁用了数据库支持，可通过编辑 src/CMakeLists.txt 去掉 #add_subdirectory(dblib) 注释开启<br>
目前只实现了 Oracle，Postgesql 的支持，其他数据库可参考 src/dblib<br>

# 配置
可通过命令行进行配置，例如 <br>
./hepollc -port 8088 -process_num 4 -reuseport 1<br>
也可通过 bin/conf/ 目录进行配置<br>
