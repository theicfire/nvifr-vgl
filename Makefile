all: libvgltrans_hello.so

clean:
	$(RM) *.so *.o nvifr-encoder/*.o nvifr-encoder/NVIFROpenGLChrome

libvgltrans_hello.so: nvifr-encoder/XCapture.o testplugin.o
	$(CXX) -O3 -g -Wall -Werror -shared -z defs -o $@ $^ -lnvidia-ifr -lGL \
		-lGLEW -lX11

nvifr-encoder/XCapture.o:
	$(MAKE) -C nvifr-encoder CXX=$(CXX)

testplugin.o: testplugin.cpp
	$(CXX) -O3 -g -Wall -Werror -fpic -I../virtualgl/include/ \
		-I../virtualgl/server/ -I../virtualgl/common/ -c -o $@ $^
