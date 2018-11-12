[中文介绍](https://github.com/liaosanity/shs/blob/master/README.zh_CN.md)

# Overview
SHS, a simple http service framework, which separates the network layer highly from the business layer code, Developers would have your own powerful HTTP services simply by implementing the business code, The network layer using non-blocking IO & Epoll all the way through, provides high concurrency and low latency network services easily.

# SHS supports several network service models, as follow:
* Multi-process single-threaded model, with the SO_REUSEPORT option, there are multiple worker-process listening the same socket-port, The kernel'll determine which available socket listener (and by implication, which worker) gets the connection, This can reduce lock contention between workers accepting new connections, and improve performance on multicore systems.
![image](https://github.com/liaosanity/shs/raw/master/images/multiprocess.png)
* Single-process multi-threaded model, The main thread deal with all of the network things, meanwhile tasker thread deal with the business logic, Threads are notified to work by pipe, which being pre-registered in the Epoll.
![image](https://github.com/liaosanity/shs/raw/master/images/singleprocess.png)

# Building
 * Environment dependence:   
   Centos 7.2  
 * Software dependence (install them all to '$(HOME)/opt/'):  
   gflags-2.0, libdaemon-0.14, cJSON-1.0.1, log4cplus-1.2.1, slog-1.0.0, boost-1.50.0 
 * Source compile:  
   make  
   make package
   tar xzvf packages/shs-1.0.0.tgz -C ~/opt

# A quick start example
There is a sub-dir name shs_test in the examples, which is a simple 'helloworld' http service, you only have to implement a few lines of code, then you have finished an http service application.
![image](https://github.com/liaosanity/shs/raw/master/images/shs_test.png)
1) Run these cmd to compile and package as bellow
make clean && make -j && make package
2) Run your application on a server like 192.168.1.11
scp packages/shs_test-1.0.0.tgz 192.168.1.11:/tmp
3) In case you set up your application under '/home/test' dir, just run these cmd as bellow
cd /home/test
tar xzvf /tmp/shs_test-1.0.0.tgz -C .
ln -s shs_test-1.0.0 shs_test
cd shs_test/tools/
./install.sh demo
./serverctl start
WARN: If you want to change '/home/test', do not forget to change them all in thes 'tools/install.sh & etc/xxx' files.
4) If you run a http request via chrome like this:
http://192.168.1.11:7007/shs_test/test?uid=Jeremy&sid=123456
Then you'll get these results as bellow:
![image](https://github.com/liaosanity/shs/raw/master/images/helloworld.png)

# A clustered service example
There is a sub-dir name proxy_test in the examples, that'll transfer the GET/POST request to shs_test, then the response will return by the way request comes. That's so simple, a proxy_test & shs_test can set up a upstream and downstream clustered service.
![image](https://github.com/liaosanity/shs/raw/master/images/ud.png)
If you run a http request via chrome like this:
http://192.168.1.11:6007/proxy_test/test?uid=Jeremy&sid=123456
Then you'll get these results as bellow:
![image](https://github.com/liaosanity/shs/raw/master/images/proxy.png)
