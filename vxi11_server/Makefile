
TARGET =test_$(notdir $(CURDIR))  
#CC=$(CROSS_COMPILE)gcc
CROSS_COMPILE = arm-arago-linux-gnueabi-
CC=$(CROSS_COMPILE)gcc
#搜索demo下所有的.c 并编译为.o 
objs := $(patsubst %c, %o, $(shell ls *.c))
$(TARGET):$(objs)
#根据需求添加库
	$(CC) -o  $@ $^ -lpthread
%.o:%.c
	$(CC) -c -o $@ $< 
clean:
	rm -f  $(TARGET) *.all *.o