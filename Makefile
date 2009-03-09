SVN ?= /usr
APR_INCLUDES ?= /usr/include/apr-1
CXXFLAGS += -I${APR_INCLUDES} -I${SVN}/include/subversion-1 -pipe -O2
LDFLAGS += -L${SVN}/lib64 -lsvn_fs-1 -lsvn_repos-1

all: svn-fast-export

svn-fast-export: committers.o svn-fast-export.o
	${CXX} $^ -o $@ ${LDFLAGS}

%.o: %.cxx
	${CXX} -c $< -o $@ ${CXXFLAGS}

.PHONY: clean

clean:
	rm -rf svn-fast-export
