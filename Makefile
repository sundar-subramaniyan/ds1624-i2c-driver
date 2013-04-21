obj-m := ds1624.o

KERNEL_DIR ?= /path/to/kernel/directory
PWD ?= $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
