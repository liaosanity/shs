
# 简介
SHS，一个简单的http服务框架，其将网络层与业务层代码高度分离，开发者仅需实现业务代码，即可拥有强大的http服务程序，网络层使用异步IO及Epoll事件驱动模式提供高并发、低延迟网络服务。

该框架支持以下几种网络服务模型：

一、多进程单线程模型（进程数可配置)，利用SO_REUSEPORT特性，允许多个进程同时监听一个端口，当有新连接请求到来，由内核决定哪个进程负责处理，从而避免多进程同时枪锁规避惊群带来性能损耗问题，整体架构如下：

![image](https://github.com/liaosanity/shs/raw/master/images/multiprocess.png)

二、单进程多线程模型（线程数可配置)，主线程负责监听及响应网络处理，任务线程负责处理业务逻辑，线程间通过管道事件进行通知，整体架构如下：

![image](https://github.com/liaosanity/shs/raw/master/images/singleprocess.png)

# 如何编译
 * 环境依赖：  
   Centos 7.2  
 * 软件依赖，把他们都安装到‘$(HOME)/opt/’目录下：  
   gflags-2.0, libdaemon-0.14, cJSON-1.0.1, log4cplus-1.2.1, slog-1.0.0, boost-1.50.0 
 * 源码编译：  
```
   make  
   make package
   tar xzvf packages/shs-1.0.0.tgz -C /home/opt
```

# 一个快速入门的例子
examples目录下有个shs_test工程，是一个简单的helloworld服务程序，仅需要实现几行业务代码便可拥有一个完整的http服务程序，如下图：

![image](https://github.com/liaosanity/shs/raw/master/images/shs_test.png)

1) 运行以下命令进行编译及打包：
```
make clean && make -j && make package
```
2) 将程序包scp到运行机器，假设为192.168.1.11：
```
scp packages/shs_test-1.0.0.tgz 192.168.1.11:/tmp
```
3) 将程序包scp到运行机器，假设为：
```
cd /home/test
tar xzvf /tmp/shs_test-1.0.0.tgz -C .
ln -s shs_test-1.0.0 shs_test
cd shs_test/tools/
./install.sh demo
./serverctl start
注意：如果你想改变部署路径'/home/test'，则记得将tools/install.sh、etc/*里的所有文件包含'/home/test'改为你要的目录。
```
4) 在chrome里输入命令 (http://192.168.1.11:7007/shs_test/test?uid=Jeremy&sid=123456), 便可与shs_test进行交互，效果如下图：

![image](https://github.com/liaosanity/shs/raw/master/images/helloworld.png)

# 一个集群服务例子
examples目录下有个proxy_test工程，一个简单的代理demo，可以向shs_test发起GET、POST请求，与shs_test组成一个上下游服务集群，架构图如下：

![image](https://github.com/liaosanity/shs/raw/master/images/ud.png)

在chrome里输入命令 (http://192.168.1.11:6007/proxy_test/test?uid=Jeremy&sid=123456), 便可与proxy_test进行交互，其会把请求透传给下游服务shs_test，并依次原路将结果返回给client，效果如下图：

![image](https://github.com/liaosanity/shs/raw/master/images/proxy.png)

你可以参照proxy_test、shs_test代码编写你的业务服务集群。
