TARGET := hid_pcmeter
SRCDIR := src
KERNEL := /lib/modules/$(shell uname -r)/build

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
