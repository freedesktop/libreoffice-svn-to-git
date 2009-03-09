SVN ?= /usr
APR_INCLUDES ?= /usr/include/apr-1
CFLAGS += -I${APR_INCLUDES} -I${SVN}/include/subversion-1 -pipe -O2 -std=c99
LDFLAGS += -L${SVN}/lib64 -lsvn_fs-1 -lsvn_repos-1

all: svn-fast-export

svn-fast-export: svn-fast-export.c

.PHONY: clean

clean:
	rm -rf svn-fast-export
