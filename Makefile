obj-m := xilinx-sw-converter.o

SRC := $(shell pwd)

EXTRA_CFLAGS := -I$(KERNEL_SRC)/drivers/media/platform/

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC)

modules_install:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC) modules_install

clean:
	rm -f *.o *~ core .depend .*.cmd *.ko *.mod.c
	rm -f Module.markers Module.symvers modules.order
	rm -f */modules.order */modules.builtin
	rm -rf .tmp_versions Modules.symvers
