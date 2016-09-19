#
#  Copyright (C) 2016, Liu Xiaofeng <lxf.junos@aliyun.com>
#  Licensed under MIT, http://opensource.org/licenses/MIT.
#
#  Makefile for the Junos kernel.
#

AS=nasm
CC=gcc

SFLAGS=-g -f elf32
CFLAGS=-g -c -x c -Wall -m32 -std=c99 -nostdinc -nostdlib -fno-builtin \
       -fno-stack-protector  -I ../../include
.s.o:
	@echo         AS	$<
	@$(AS) $(SFLAGS) -o $@  $<
.c.o:
	@echo         CC	$<
	@$(CC) $(CFLAGS) -o $@  $<

SRCS = $(wildcard *.c GUI/*/*.c)  
OBJS = $(patsubst %c, %o, $(SRCS))

CFLAGS+=-I . \
        -I GUI/core\
        -I GUI/WM   \
        -I GUI/Widget


ucGUI.a:GUI_main.o LCDvga.o $(OBJS) 
	@echo ar ucGUI.a 
	@ar rcs   ucGUI.a  $(OBJS)

clean:
	@rm -f  ucGUI.a $(OBJS)