#CXXFLAGS += -pipe -O0 -g #-O2
CXXFLAGS += -O2

SVN ?= /usr
APR_INCLUDES ?= /usr/include/apr-1
SVN_CXXFLAGS += ${CXXFLAGS} -I${APR_INCLUDES} -I${SVN}/include/subversion-1
SVN_LDFLAGS = ${LDFLAGS} -L${SVN}/lib64 -lsvn_fs-1 -lsvn_repos-1

HG_CXXFLAGS += ${CXXFLAGS} `python-config --includes`
HG_LDFLAGS = ${LDFLAGS} `python-config --libs` -lboost_python

all: svn-fast-export hg-fast-export

svn-fast-export: committers.o error.o filter.o repository.o svn-fast-export.o
	${CXX} $^ -o $@ ${SVN_LDFLAGS}

hg-fast-export: committers.o error.o filter.o repository.o hg-fast-export.o
	${CXX} $^ -o $@ ${HG_LDFLAGS}

svn-fast-export.o: svn-fast-export.cxx
	${CXX} -c $< -o $@ ${SVN_CXXFLAGS}

hg-fast-export.o: hg-fast-export.cxx
	${CXX} -c $< -o $@ ${HG_CXXFLAGS}

%.o: %.cxx
	${CXX} -c $< -o $@ ${CXXFLAGS}

.PHONY: clean

clean:
	rm -rf svn-fast-export svn-fast-export.o
	rm -rf hg-fast-export hg-fast-export.o
	rm -rf committers.o error.o filter.o repository.o
