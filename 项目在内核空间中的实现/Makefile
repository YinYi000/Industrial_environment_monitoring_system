ifneq ($(KERNELRELEASE),)
	obj-m += rtdm-project-noyau.o

        EXTRA_CFLAGS := -I /usr/xenomai/include/
else

	XENOCONFIG=/usr/xenomai/bin/xeno-config
	CC=$(shell      $(XENOCONFIG) --cc)
	CFLAGS=$(shell  $(XENOCONFIG) --skin=posix --cflags)
	LDFLAGS=$(shell $(XENOCONFIG) --skin=posix --ldflags)
        LIBDIR=$(shell  $(XENOCONFIG) --skin=posix --libdir)

        CROSS_COMPILE ?=
        KERNEL_DIR ?= ~/TP1/linux
        MODULE_DIR := $(shell pwd)

.PHONY: all
all:: modules executable

.PHONY: modules
modules:
	$(MAKE) -C $(KERNEL_DIR) SUBDIRS=$(MODULE_DIR) CROSS_COMPILE=$(CROSS_COMPILE) modules

.PHONY: executable
executable: LCD_utilisateur

XENOCONFIG=/usr/xenomai/bin/xeno-config

LCD_utilisateur: LCD_utilisateur.c
	$(CC) -c -o LCD_utilisateur.o LCD_utilisateur.c $(CFLAGS) $(LDFLAGS) 
	/usr/xenomai/bin/wrap-link.sh -v $(CC) -o LCD_utilisateur LCD_utilisateur.o $(LDFLAGS) -lwiringPi

.PHONY: clean
clean::
	rm -f  *.o  .*.o  .*.o.* *.ko  .*.ko  *.mod.* .*.mod.* .*.cmd *~
	rm -f Module.symvers Module.markers modules.order
	rm -rf .tmp_versions
	rm -f LCD_utilisateur
endif

