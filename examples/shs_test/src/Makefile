include ../version

# get CPUS
PLATFORM := $(shell uname)
CPUS := $(strip $(if $(shell echo $(PLATFORM)|grep Linux),\
	$(shell cat /proc/cpuinfo|grep -c processor),\
	$(shell sysctl -a | egrep -i 'hw.ncpu' | cut -d: -f2)))

BOOST=$(HOME)/opt/boost-$(BOOST_VERSION)
CJSON=$(HOME)/opt/cJSON-1.0.1
SLOG=$(HOME)/opt/slog-1.0.0
SHS=$(HOME)/opt/shs-$(SHS_VERSION)

CXXFLAGS := -g3 -O2 -std=c++11 -fno-strict-aliasing -Wall -Wno-deprecated -Wno-sign-compare -fPIC \
	-I$(BOOST)/include \
        -I $(CJSON)/include \
	-I$(SHS)/include \
	-I$(SLOG)/include 

LDFLAGS := -pthread \
	-L$(BOOST)/lib \
        -L $(CJSON)/lib \
	-L$(SHS)/lib \
	-L$(SLOG)/lib

RTFLAGS := -Wl,-rpath=.

LIBS := -lpthread \
        -lcjson \
	-lslog \
	-lshs

SRC := $(wildcard *.cc) 
OBJ := $(patsubst %.cc, %.o, $(SRC))
DEP := $(patsubst %.o, %.d, $(OBJ))

MAJOR=1
MINOR=$(MOD_VERSION)

TARGET := $(LIB_NAME)
ifeq ($(USE_DEP),1)
-include $(DEP) $(GEN_DEP)
endif

all:
	$(MAKE) -j$(CPUS) USE_DEP=1 target

target: $(TARGET)

$(LIB_NAME): $(OBJ)
	$(CXX) $^ -Wl,-soname,$(@).$(MAJOR) -o $@.$(MAJOR).$(MINOR) $(RTFLAGS) $(LDFLAGS) $(LIBS) -shared
	rm -fr $(@)
	rm -fr $(@).$(MAJOR)
	/sbin/ldconfig -n .
	ln -s $(@).$(MAJOR) $(@)

%.o : %.cc
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.d : %.cc
	@$(CXX) -MM $< $(CXXFLAGS) | sed 's/$(notdir $*)\.o/$(subst /,\/,$*).o $(subst /,\/,$*).d/g' > $@

clean:
	-rm -rf $(OBJ) $(TARGET) $(DEP) $(GEN_DEP) *.so.*

test: all

PACKAGE_NAME=$(MOD_NAME)-$(MOD_VERSION)
install:
	/usr/bin/install -c $(TARGET).$(MAJOR).$(MINOR) ../packages/$(PACKAGE_NAME)/module
	/sbin/ldconfig -n ../packages/$(PACKAGE_NAME)/module
	(cd ../packages/$(PACKAGE_NAME)/module; ln -s $(TARGET).$(MAJOR) $(TARGET))

.PHONY: all target clean test

