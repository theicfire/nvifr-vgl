VGL := ../virtualgl
CXXFLAGS := -O3 -g -Wall -Werror -fpic

all: libvgltrans_hello.so

clean:
	$(RM) *.so *.o nvifr-encoder/*.o nvifr-encoder/NVIFROpenGLChrome

libvgltrans_hello.so: nvifr-encoder/XCapture.o testplugin.cpp \
	$(VGL)/build/lib/libvglutil.a
	$(CXX) $(CXXFLAGS) -shared -z defs -Wl,--version-script,testplugin-mapfile \
		-o $@ $^ -I$(VGL)/common -I$(VGL)/include -I$(VGL)/server -lnvidia-ifr \
		-lGL -lX11 -lpthread

nvifr-encoder/XCapture.o:
	$(MAKE) -C nvifr-encoder CXX=$(CXX)
