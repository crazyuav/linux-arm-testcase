# make all demoes at one time

#arm-linux for am335x use arm-linux-gnueabihf-
CROSS_COMPILE = arm-linux-gnueabihf-
CC=$(CROSS_COMPILE)gcc
#linux pc --use gcc
#CROSS_COMPILE =

all:
# demo
	make -C ./coredump/
	make -C ./scpi_vxi_tmc/
	make -C ./test/
	make -C ./usbtest/
	make -C ./usb/
	make -C ./vxi11_server/
    
.Phony: clean

clean:
	make clean -C ./coredump/
	make clean -C ./scpi_vxi_tmc/
	make clean -C ./test/
	make clean -C ./usbtest/
	make clean -C ./usb/
	make clean -C ./vxi11_server/