all:
	cd src && $(MAKE) $@

dynamic:
	cd src && $(MAKE) $@

static:
	cd src && $(MAKE) $@

install:
	cd src && $(MAKE) $@

uninstall:
	cd src && $(MAKE) $@

clean:
	cd src && $(MAKE) $@

.PHONY: all dynamic static install uninstall clean
