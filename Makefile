CPPFLAGS=$(shell [ "`uname -s`" = "Darwin" ] && echo '-Dsocklen_t=int -DMSG_NOSIGNAL=0 -I/sw/include')
LDFLAGS=$(shell [ "`uname -s`" = "Darwin" ] && echo '-L/sw/lib -lpoll')
export CPPFLAGS
export LDFLAGS

all:
	cd src && $(MAKE) $(MAKEFLAGS)

clean:
	cd src && $(MAKE) $(MAKEFLAGS) clean
