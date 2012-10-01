obj-m = gpio-remote.o
KVERSION=3.5.0-212-omap4
INCLUDEDIR = /usr/src/linux-headers-$(VERSION)/arch/arm/plat-omap/include

all:
	make -I $(INCLUDEDIR) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
