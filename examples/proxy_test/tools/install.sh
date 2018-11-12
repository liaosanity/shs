#/bin/sh

if [ $# -ne 1 ]
then
    echo "Usage: sh `basename $0` debug|demo|test"
    exit -1
fi

arg=$1
host=""
log=""
cluster=""

if [ $arg = "debug" ]
then
    host="debug"
    log="local"
elif [ $arg = "demo" ]
then
    host="demo"
    log="demo"
elif [ $arg = "test" ]
then
    host="demo"
    log="ol"
else
    echo "Usage: sh `basename $0` debug|demo|test"
    exit -1
fi

# etc
if [ ${host} = "debug" ] || [ ${host} = "demo" ]
then
    ln -sf /home/test/proxy_test/etc/proxy_test_${host}.conf /home/test/proxy_test/etc/proxy_test.conf
else
    ln -sf /home/test/proxy_test/etc/proxy_test_${host}_${cluster}.conf /home/test/proxy_test/etc/proxy_test.conf
fi

# logging
ln -sf /home/test/proxy_test/etc/logging.conf.$log /home/test/proxy_test/etc/logging.conf

# lib
/sbin/ldconfig -n /home/test/proxy_test/lib > /dev/null 2>&1

# module
/sbin/ldconfig -n /home/test/proxy_test/module > /dev/null 2>&1
ln -sf /home/test/proxy_test/module/libproxy_test.so.1 /home/test/proxy_test/module/libproxy_test.so

