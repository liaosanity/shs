# get CPUS
PLATFORM := $(shell uname)
CPUS := $(strip $(if $(shell echo $(PLATFORM)|grep Linux),\
	$(shell cat /proc/cpuinfo|grep -c processor),\
	$(shell sysctl -a | egrep -i 'hw.ncpu' | cut -d: -f2)))

CJSON=$(HOME)/opt/cJSON-1.0.1/lib
LOG4CPLUS=$(HOME)/opt/log4cplus-1.2.1/lib
SLOG=$(HOME)/opt/slog-1.0.0/lib

VERSION=1.0.0
TARGET := shs

$(TARGET): 
	$(MAKE) -C src

clean:
	rm -rf packages
	$(MAKE) clean -C src

PACKAGE_DIRS=etc module log proc bin lib \
	 include/http \
	 include/downstream \
	 include/comm \
	 include/core

PACKAGE_NAME=$(TARGET)-$(VERSION)

install: $(TARGET)
	@if [ -z "$(VERSION)" ]; then \
		echo "ERROR: VERSION is not set"; \
		exit 1; \
	fi
	@if test ! -d packages/$(PACKAGE_NAME); then \
		mkdir -p packages/$(PACKAGE_NAME); \
	else \
		rm -fr packages/$(PACKAGE_NAME); \
	fi
	@for d in $(PACKAGE_DIRS); do \
		if test ! -d packages/$(PACKAGE_NAME)/$$d; then mkdir -p packages/$(PACKAGE_NAME)/$$d; fi \
	done
	/usr/bin/install -c src/shs packages/$(PACKAGE_NAME)/bin
	/usr/bin/install -c src/libshs.so packages/$(PACKAGE_NAME)/lib
	/usr/bin/install -c $(CJSON)/libcjson.so packages/$(PACKAGE_NAME)/lib
	/usr/bin/install -c $(LOG4CPLUS)/liblog4cplus.so packages/$(PACKAGE_NAME)/lib
	/usr/bin/install -c $(SLOG)/libslog.so packages/$(PACKAGE_NAME)/lib
	/sbin/ldconfig -n packages/$(PACKAGE_NAME)/lib
	/usr/bin/install -m 664 -c src/http/*.h packages/$(PACKAGE_NAME)/include/http/
	/usr/bin/install -m 664 -c src/types.h packages/$(PACKAGE_NAME)/include/
	/usr/bin/install -m 664 -c src/module.h packages/$(PACKAGE_NAME)/include/
	/usr/bin/install -m 664 -c src/output.h packages/$(PACKAGE_NAME)/include/
	/usr/bin/install -m 664 -c src/event_watcher.h packages/$(PACKAGE_NAME)/include/
	/usr/bin/install -m 664 -c src/http/invoke_params.h packages/$(PACKAGE_NAME)/include/
	/usr/bin/install -m 664 -c src/http/http_invoke_params.h packages/$(PACKAGE_NAME)/include/
	/usr/bin/install -m 664 -c src/comm/*.h packages/$(PACKAGE_NAME)/include/comm/
	/usr/bin/install -m 664 -c src/core/*.h packages/$(PACKAGE_NAME)/include/core/
	/usr/bin/install -m 664 -c src/downstream/*.h packages/$(PACKAGE_NAME)/include/downstream/

package: clean install
	rm -fr packages/$(PACKAGE_NAME).tgz
	tar zcf packages/$(PACKAGE_NAME).tgz -C packages $(PACKAGE_NAME)

.PHONY: target clean 
