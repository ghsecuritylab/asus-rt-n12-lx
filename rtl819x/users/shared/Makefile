#
# Linux router shared code Makefile
#
# Copyright 2005, ASUSTek Inc.
# All Rights Reserved.
# 
# THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
# KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
# SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
# FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
#
# $Id: Makefile,v 1.1 2011/02/25 05:32:42 mars_zhang Exp $
#

CFLAGS	+= -I. -I./include -I../httpd -Wall -DASUS_NVRAM -D__CONFIG_NAT__ -DDLM -I../../linux-2.6.30/include

CFLAGS	+= -s
#CFLAGS	 += -DW7_LOGO
#CFLAGS  += -DW7_WIFI
#CFLAGS  += -DMR
LDFLAGS += -L.

# 2008.10 magic. add notify_rc.o
#OBJS := shutils.o crc.o http.o defaults.o nvram_linux.o linux_timer.o nvparse.o notify_rc.o bin_sem_asus.o 
#OBJS += semaphore.o flash_mtd.o pids.o files.o
OBJS := shutils.o defaults.o nvram_linux.o  

all: libshared.so

romfs:
	$(ROMFSINST) libshared.so /lib/libshared.so
	#$(STRIP) $(INSTALLDIR)/lib/libpthread-0.9.28.so
	#$(STRIP) $(INSTALLDIR)/lib/libstdc++-0.2.2.so

clean:
	rm -f *.o libshared.so

libshared.so: $(OBJS)
	$(LD) -shared -o $@ $^
	$(STRIP) --strip-debug $@
