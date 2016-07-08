CPPFLAGS = -std=c++11 -fpic -pthread -O2 -g -fno-strict-aliasing -flto=8 -fwrapv -Wall
LDLIBS = -luv

SRC=$(wildcard *.cpp)
TARGET=$(patsubst %.cpp,%,$(SRC))

all: $(TARGET)

clean:
	rm -rf $(TARGET)
