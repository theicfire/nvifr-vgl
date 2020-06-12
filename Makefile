VGL := ../virtualgl
CXXFLAGS := -O3 -g -Wall -Werror -fpic

all: libvgltrans_hello.so mini_mighty


# TODO do I need -pthread?
shared_mem_comm.o: shared_mem_comm.cpp
	$(CXX) -c $(CXXFLAGS) shared_mem_comm.cpp -o shared_mem_comm.o

libvgltrans_hello.so: nvifr-encoder/XCapture.o testplugin.cpp \
	$(VGL)/build/lib/libvglutil.a shared_mem_comm.o
	$(CXX) $(CXXFLAGS) -shared -z defs -Wl,--version-script,testplugin-mapfile \
		-o $@ $^ -I$(VGL)/common -I$(VGL)/include -I$(VGL)/server -lnvidia-ifr \
		-lGL -lX11 -lpthread -lrt

nvifr-encoder/XCapture.o:
	$(MAKE) -C nvifr-encoder CXX=$(CXX)

mini_mighty.o: mini_mighty.cpp
	$(CXX) -c $(CXXFLAGS) mini_mighty.cpp -o mini_mighty.o

mini_mighty: shared_mem_comm.o mini_mighty.o
	$(CXX) $(CXXFLAGS) -o $@ $^ -lrt -lpthread 

clean:
	$(RM) *.so *.o nvifr-encoder/*.o nvifr-encoder/NVIFROpenGLChrome mini_mighty