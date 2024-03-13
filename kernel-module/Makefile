TARGET := hid_pcmeter
SRCDIR := src
KERNEL := /lib/modules/$(shell uname -r)/build
MODDESTDIR := /lib/modules/$(shell uname -r)/kernel/drivers/hid/

obj-m := $(TARGET).o
$(TARGET)-y += $(SRCDIR)/hid_pcmeter.o

HEADERS := $(PWD)/include
ccflags-y := -I$(HEADERS)
ccflags-y += -Wall

all: module

module:
	make -C $(KERNEL) M=$(PWD) modules

clean:
	make -C $(KERNEL) M=$(PWD) clean

install:
	install -p -m 644 $(TARGET).ko  $(MODDESTDIR)
	depmod -a $(shell uname -r)

uninstall:
	rm -f $(MODDESTDIR)/$(TARGET).ko
	depmod -a $(shell uname -r)
