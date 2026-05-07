# Makefile for the messenger tools
#
# IMPORTANT: Run with make -R (or make --no-builtin-variables) if CC etc are 
# not defined in the environment or the make command line as otherwise the 
# built-in values will be used rather than the defaults given below.
#

CC          ?= gcc
CFLAGS      ?= -march=native -O2 -std=c99
DEFINES     ?= -D_GNU_SOURCE=
LIBS        ?= 

.SUFFIXES:

exe : bindir \
	  ../bin/pushover \
	  ../bin/pushslack

clean : 
	-/bin/rm ../bin/*
	
bindir :
	-mkdir -p ../bin

get-inih :
	curl -O https://raw.githubusercontent.com/benhoyt/inih/refs/heads/master/ini.c
	curl -O https://raw.githubusercontent.com/benhoyt/inih/refs/heads/master/ini.h
	curl -o LICENSE-inih.txt https://raw.githubusercontent.com/benhoyt/inih/refs/heads/master/LICENSE.txt
	
pushover : ../bin/pushover
../bin/pushover : pushover.c Makefile ini.c ini.h
	$(CC) $(CFLAGS) $(DEFINES) ini.c $< -lcurl $(LIBS) -o $@

slack : ../bin/pushslack
../bin/pushslack : pushslack.c Makefile ini.c ini.h
	$(CC) $(CFLAGS) $(DEFINES) ini.c $< -lcurl $(LIBS) -o $@
