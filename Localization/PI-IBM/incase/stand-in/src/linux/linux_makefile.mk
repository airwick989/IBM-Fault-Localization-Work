PATH_SEP=/

KERNEL_DIR=linux

_KERNEL_DEPS=linux_kernel.h
KERNEL_DEPS=$(patsubst %,$(SRC)$(PATH_SEP)$(KERNEL_DIR)$(PATH_SEP)%,$(_KERNEL_DEPS))

_KERNEL_OBJ=linux_kernel.o
KERNEL_OBJ=$(patsubst %,$(ODIR)$(PATH_SEP)$(KERNEL_DIR)$(PATH_SEP)%,$(_KERNEL_OBJ))

CC=gcc
AR=ar
LNK=g++
CFLAGS=-I$(SRC) -fPIC -Wall
LFLAGS=--shared

RM=rm -rf
MKDIR=mkdir
