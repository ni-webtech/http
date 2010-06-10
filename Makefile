# 
#	Makefile -- Top level Makefile for the Http library 
#
#	Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
#
#
#	Standard Make targets supported are:
#	
#		make 						# Does a "make compile"
#		make clean					# Removes generated objects
#		make compile				# Compiles the source
#		make depend					# Generates the make dependencies
#		make test 					# Runs unit tests
#

include		build/make/Makefile.top

diff import sync:
	if [ ! -x $(BLD_TOOLS_DIR)/edep$(BLD_BUILD_EXE) -a "$(BUILDING_CROSS)" != 1 ] ; then \
		$(MAKE) -S --no-print-directory _RECURSIVE_=1 -C $(BLD_TOP)/build/src compile ; \
	fi
	if [ "`git branch`" != "* master" ] ; then echo "Sync only in default branch" ; echo 255 ; fi
	import.ksh --$@ --src ../tools --dir . ../tools/build/export/export.gen
	import.ksh --$@ --src ../tools --dir . ../tools/build/export/export.configure
	import.ksh --$@ --src ../mpr 	--dir . ../mpr/build/export/export.gen
	import.ksh --$@ --src ../mpr 	--dir ./src/include --strip ./all/ ../mpr/build/export/export.h
	import.ksh --$@ --src ../mpr 	--dir ./src/deps/mpr --strip ./all/ ../mpr/build/export/export.c
	echo

