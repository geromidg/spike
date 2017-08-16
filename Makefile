########################################
# Makefile for RTES Exercise 3	       #
# 				       #
# Kostas Mylonakis -- mylonakk@auth.gr #
########################################

#Note: searchWifi.sh works only on zSun, probably not in any OS.

#remember to change this for cross-compile
CC=gcc

#remember to upload coresponding lib @zSun:/lib
LIBS=-lpthread

RM=rm -f

all: prod-cons callScript

prod-cons: prod-cons.c
	$(CC) $< -o $@ $(LIBS)

callScript: callScript.c
	$(CC) $< -o $@ $(LIBS)

clean:
	$(RM) prod-cons callScript
