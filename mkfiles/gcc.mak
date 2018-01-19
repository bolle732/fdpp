#
# GCC.MAK - kernel copiler options for gcc
#

DIRSEP=/
RM=rm -f
CP=cp
ECHOTO=../utils/echoto
CC=g++ -c -std=c++11
CL=gcc

TARGETOPT=
ifneq ($(XCPU),386)
$(error unsupported CPU 186)
endif

TARGET=KMS

ALLCFLAGS:=-I../hdr -I. $(TARGETOPT) $(ALLCFLAGS) -Wall -fpic -O2 \
    -ggdb3 -fno-strict-aliasing

INITCFLAGS=$(ALLCFLAGS)
CFLAGS=$(ALLCFLAGS)
CLDEF = 1
CLC = gcc
CLT = gcc
INITPATCH=@echo > /dev/null
