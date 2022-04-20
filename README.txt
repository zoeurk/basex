soit
make -f Makefile.src
ou:
aclocal
autoconf
autoreconf --install
./configure --prefix=/usr --exec-prefix=/usr
make
make install
