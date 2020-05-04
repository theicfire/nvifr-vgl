set -ex

export LD_LIBRARY_PATH=/home/mighty
g++ -I../include/ -I../common/ -I/opt/libjpeg-turbo/include/ -I../build/include/  -c -Wall -fpic testplugin.cpp
g++ -shared -o ~/libvgltrans_hello.so testplugin.o
VGL_VERBOSE=1; vglrun -v -trans hello -d :1 glxgears
