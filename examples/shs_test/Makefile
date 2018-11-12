include version

SHS=$(HOME)/opt/shs-$(SHS_VERSION)

all: 
	$(MAKE) all -C src

clean:
	$(MAKE) clean -C src
	rm -rf ./packages

PACKAGE_NAME=${MOD_NAME}-$(MOD_VERSION)

install: 
	@if test ! -d packages/$(PACKAGE_NAME); then \
		mkdir -p packages/$(PACKAGE_NAME); \
	else \
	    rm -rf packages/$(PACKAGE_NAME); \
	    mkdir -p packages/$(PACKAGE_NAME); \
	fi
	mkdir -p packages/$(PACKAGE_NAME)/tools 
	cp -R $(SHS)/* packages/$(PACKAGE_NAME)
	rm -fr packages/$(PACKAGE_NAME)/include
	/usr/bin/install -c version packages/$(PACKAGE_NAME)/bin
	/usr/bin/install -c tools/* packages/$(PACKAGE_NAME)/tools
	cp -fr etc packages/$(PACKAGE_NAME)
	$(MAKE) install -C src VERSION=$(MOD_VERSION)
	mv packages/$(PACKAGE_NAME)/bin/shs packages/$(PACKAGE_NAME)/bin/${BIN_NAME}
	find packages/$(PACKAGE_NAME) -name ".svn" | xargs rm -fr
	find packages/$(PACKAGE_NAME) -type l|xargs rm -rf

package: install
	tar zcf packages/${MOD_NAME}-$(MOD_VERSION).tgz -C packages $(PACKAGE_NAME)

.PHONY: all target clean test

