set -ex

export LD_LIBRARY_PATH=`pwd`
g++ -I../virtualgl-2.6.3/include/ -I../virtualgl-2.6.3/common/ -I/opt/libjpeg-turbo/include/ -I../virtualgl-2.6.3/build/include/ \
-c -Wall -fpic testplugin.cpp

g++ -shared \
  -o `pwd`/libvgltrans_hello.so \
  -m64  testplugin.o nvifr-encoder/XCapture.o  nvifr-encoder/encoder.o  -lnvidia-ifr -lGL -lGLEW -lX11 

# g++ -shared \
#  -o ~/libvgltrans_hello.so testplugin.o encoder.o XCapture.o \
# -lGL \
# -lGLEW \
# -lX11 \
# -lXtst \
# -lXinerama \
# -lxkbcommon \
# -lnvidia-ifr

VGL_VERBOSE=1; vglrun -v -trans hello -d :1 glxgears
