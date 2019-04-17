MPATH = ../module/
SRCS = $(wildcard *.cpp) \
$(wildcard $(MPATH)cvTools/*.cpp) \
$(wildcard $(MPATH)clahe/*.cpp) \
$(wildcard $(MPATH)defog/*.cpp) \
$(wildcard $(MPATH)defog_interface/*.cpp)

PURESRCS = $(notdir $(SRCS))
OBJS = $(patsubst %.cpp,%.o,$(PURESRCS))
POBJS = $(filter-out, $(OBJS))
APPS = libdefog.so

CC = arm-buildroot-linux-uclibcgnueabi-gcc
IPATH = -I../thirdparty/opencv3.2.0/include \
-I../thirdparty/jsoncpp1.8.0/include \
-I../module/utils \
-I../module/cvTools \
-I../module/clahe \
-I../module/defog \
-I../module/defog_interface \
-I../module/neon

LPATH = -L../thirdparty/opencv3.2.0/lib  -L../thirdparty/jsoncpp1.8.0/lib
RPATH =
DLLPATH =
LIBS = -lopencv_core -lopencv_imgproc -lopencv_highgui -lopencv_imgcodecs -lopencv_videoio -ljsoncpp -lpthread
CFLAGS = -O3 -march=armv7-a -mcpu=cortex-a9 -mfpu=neon -ftree-vectorize -U__STRICT_ANSI__ -DPIPELINE -DCONTRAST_ENHANCE
LDFLAGS = -Wl,-rpath=$(RPATH)

.PHONY: $(APPS) all
all: $(APPS)

libdefog.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LPATH) $(LIBS) $(LDFLAGS)

%.o: %.cpp
	$(CC) -fPIC -g -c $^ $(IPATH) $(CFLAGS)

%.o: $(MPATH)cvTools/%.cpp
	$(CC) -fPIC -g -c $^ $(IPATH) $(CFLAGS)

%.o: $(MPATH)clahe/%.cpp
	$(CC) -fPIC -g -c $^ $(IPATH) $(CFLAGS)

%.o: $(MPATH)defog/%.cpp
	$(CC) -fPIC -g -c $^ $(IPATH) $(CFLAGS)

%.o: $(MPATH)defog_interface/%.cpp
	$(CC) -fPIC -g -c $^ $(IPATH) $(CFLAGS)

files:
	@echo srcs: $(SRCS)
	@echo pure srcs: $(PURESRCS)
	@echo objs: $(OBJS)
	@echo pobjs: $(POBJS)

install:
	ar rs libdefog2.a color.o clahe.o clhe.o defog.o defog_interface.o
	cp libdefog2.a /home/zlttest/workspace/imx6/build/rootfs_uClibc/usr/lib

clean:
	rm *.o *.a *.so
