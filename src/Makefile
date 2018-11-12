# get CPUS
PLATFORM := $(shell uname)
PLATFORM_VERSION := $(shell uname -r| awk -F 'el' '{printf("el%d", substr($$2,1,1))}')
CPUS := $(strip $(if $(shell echo $(PLATFORM)|grep Linux),\
	$(shell cat /proc/cpuinfo|grep -c processor),\
	$(shell sysctl -a | egrep -i 'hw.ncpu' | cut -d: -f2)))
GCC_VERSION := $(shell gcc --version | grep GCC | awk '{print $$3}')

GFLAGS=$(HOME)/opt/gflags-2.0
LIBDAEMON=$(HOME)/opt/libdaemon-0.14
CJSON=$(HOME)/opt/cJSON-1.0.1
LOG4CPLUS=$(HOME)/opt/log4cplus-1.2.1
BOOST=$(HOME)/opt/boost-1.50.0
SLOG=$(HOME)/opt/slog-1.0.0

CXXFLAGS := -g3 -O2 -Wall -fno-strict-aliasing \
    -Wno-error=unused-but-set-variable -Wno-error=unused-but-set-parameter \
    -fPIC \
    -std=c++0x \
    -DUSE_SO_REUSEPORT \
    -I . \
    -I $(GFLAGS)/include \
    -I $(LIBDAEMON)/include \
    -I $(CJSON)/include \
    -I $(LOG4CPLUS)/include \
    -I $(SLOG)/include \
    -I $(BOOST)/include

LDFLAGS := -pthread \
	-L. \
	-L $(CJSON)/lib \
	-L $(LOG4CPLUS)/lib \
	-L $(SLOG)/lib

RTFLAGS := \
	-Wl,-rpath=.

LIBS := -ldl \
	-lrt \
	-lcrypto \
	-lcjson \
	-llog4cplus \
	-lslog \
	$(BOOST)/lib/libboost_thread.a \
	$(BOOST)/lib/libboost_chrono.a \
	$(BOOST)/lib/libboost_system.a \
	$(BOOST)/lib/libboost_filesystem.a \
	$(GFLAGS)/lib/libgflags.a \
	$(LIBDAEMON)/lib/libdaemon.a 

SRC := $(wildcard *.cc) \
	$(wildcard core/*.cc) \
	$(wildcard comm/*.cc) \
	$(wildcard log/*.cc) \
	$(wildcard service/*.cc) \
	$(wildcard http/*.cc) \
	$(wildcard downstream/*.cc) 

OBJ := $(patsubst %.cc, %.o, $(SRC))
DEP := $(patsubst %.o, %.d, $(OBJ))

TARGET := shs libshs.so

ifeq ($(USE_DEP),1)
-include $(DEP) 
endif

all:
	sh gen_version.sh $(GCC_VERSION) $(PLATFORM_VERSION)
	$(MAKE) -j$(CPUS) USE_DEP=1 target

shs: shs.o libshs.so
	$(CXX) $^ -o $@ $(RTFLAGS) $(LDFLAGS) -lshs $(LIBS)

libshs.so: $(filter-out shs.o, $(OBJ))
	$(CXX) $^ -o $@ $(RTFLAGS) $(LDFLAGS) $(LIBS) -shared

target: $(TARGET)

%.o : %.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@
%.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.d : %.cc
	@$(CXX) -MM $< $(CXXFLAGS) | sed 's/$(notdir $*)\.o/$(subst /,\/,$*).o $(subst /,\/,$*).d/g' > $@
%.d : %.cpp
	@$(CXX) -MM $< $(CXXFLAGS) | sed 's/$(notdir $*)\.o/$(subst /,\/,$*).o $(subst /,\/,$*).d/g' > $@

clean:
	-rm -rf $(OBJ) $(TARGET) *.pid *.log *.core $(DEP) *.so ./version.h ./version.cc

.PHONY: all target clean
