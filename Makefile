TARGET_MODULE:=character-device

obj-m += $(TARGET_MODULE).o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	$(CC) user-space.c -o test
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm test
load:
	insmod ./$(TARGET_MODULE).ko

unload:
	rmmod ./$(TARGET_MODULE).ko
