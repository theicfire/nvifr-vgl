set -ex

exec vglrun -ld `pwd` -trans hello -d :1 google-chrome --disable-gpu-sandbox
