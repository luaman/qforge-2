include Makefile

ifeq ($(ACLOCAL),)

all: Makefile
	$(MAKE) -f Makefile $(MAKECMDGOALS)

.FORCED: all

Makefile: configure
	./configure --enable-maintainer-mode --prefix=$(HOME)/quake2

configure: Makefile.in configure.in config.h.in
	autoconf

Makefile.in: ltmain.sh configure.in Makefile.am config.h.in
	automake --foreign --add-missing --copy

ltmain.sh: config.h.in configure.in Makefile.am
	libtoolize --copy --automake

config.h.in: configure.in aclocal.m4
	autoheader

aclocal.m4: configure.in
	aclocal

endif
