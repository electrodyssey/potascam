all:
	gcc -I/usr/include/libusb-1.0  -o potascam potascam.c  -lusb-1.0


clean:
	rm -f potascam 
