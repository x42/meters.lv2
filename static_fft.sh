#!/bin/sh
set -e
curl -L http://www.fftw.org/fftw-3.3.4.tar.gz | tar xz
cd fftw-3.3.4/
CFLAGS="-fvisibility=hidden -fPIC -Wl,--exclude-libs,ALL" \
	./configure \
	--enable-single --enable-sse --enable-avx --disable-mpi \
	--disable-shared --enable-static
make -j2
