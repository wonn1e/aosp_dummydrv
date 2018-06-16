# Makefile - Dummy Device Driver.
#

ARCH ?= arm
KDIR ?= /lib/modules/$(shell uname -r)/build
OSKOS=linux

# Define host system directory
KDIR-$(shell uname -m):=/lib/modules/$(shell uname -r)/build
include $(KDIR)/.config

ifeq ($(ARCH), arm)
# when compiling for ARM we're cross compiling
export CROSS_COMPILE ?= $(call check_cc2, arm-linux-gnueabi-gcc, arm-linux-gnueabi-, arm-none-linux-gnueabi-)
endif

export CONFIG_DUMMY_DRV=m


h_dir = arch/$(ARCH)/include/

default:
#	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) modules
#	$(MAKE) -C $(KDIR) M=$(CURDIR) modules
#	$(MAKE) -C $(h_dir) $(KDIR) M=$(PWD) modules
	$(MAKE) -C $(h_dir) SUBDIRS=$(PWD) modules
clean:
#	$(MAKE) ARCH=$(ARCH) -C $(KDIR) M=$(CURDIR) clean
#	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
#	$(MAKE) -C $(h_dir) $(KDIR) M=$(PWD) clean
	$(MAKE) -C $(h_dir) SUBDIRS=$(PWD) clean
