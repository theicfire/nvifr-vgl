include utils.mk

TARGET = NVIFROpenGLChrome
ARCH = $(shell getconf LONG_BIT)

CXX = g++
CXXFLAGS = -g -Wall -DGLEW_STATIC
LIBS = -lnvidia-ifr -lGL -lGLEW -lX11 

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.cpp, %.o, $(wildcard *.cpp))
HEADERS = $(wildcard *.h)

%.o: %.cpp $(HEADERS)
	$(CXX) -fpic -m$(OS_SIZE) $(CXXFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CXX) -m$(OS_SIZE) $(OBJECTS) $(LINKFLAGS_$(ARCH)) $(LIBS) -o $@

clean:
	rm -f *.o
	rm -f $(TARGET)
