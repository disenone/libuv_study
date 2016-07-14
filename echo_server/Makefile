CXXFLAGS = -std=c++11 -fpic -pthread -O2 -g -fno-strict-aliasing -flto=8 -fwrapv -Wall
CFLAGS = -std=gnu11 -fpic -pthread -O2 -g -fno-strict-aliasing -flto=8 -fwrapv -Wall
LDLIBS = -luv -lev

CPPSRC = $(wildcard *.cpp)
CSRC = $(wildcard *.c)
TARGET = $(patsubst %.cpp,%,$(CPPSRC)) $(patsubst %.c,%,$(CSRC))

all: $(TARGET)

clean:
	rm -rf $(TARGET)
