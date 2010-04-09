#
#	Makefile for the all-in-one HTTP distribution
#
#	Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
#

include			.makedep

COMPILE			=
TARGETS			+= httpLib.c http.h http.c 

compileExtra: 	$(TARGETS) dummy

#
#	Order of headers matters
#
HEADERS			+= ../src/include/http.h
HTTP_SOURCES 	+= $(wildcard ../src/*.c) 

http.h: Makefile $(HEADERS)
	$(call log) "[Generate]" all/http.h
	all-in-one $(HEADERS) | egrep -v '#include.*http|#inc.*config.h|#inc.*ucp.*\.h|#inc.*pcre' >http.h

httpLib.c: Makefile $(HEADERS) $(HTTP_SOURCES)
	$(call log) "[Generate]" all/httpLib.c
	echo '#include "http.h"' >httpLib.c
	all-in-one $(HTTP_SOURCES) | egrep -v '#inc.*http' >>httpLib.c

http.c: Makefile ../src/utils/httpCmd.c
	$(call log) "[Generate]" all/http.c
	cp ../src/utils/httpCmd.c http.c

cleanExtra:
	rm -f *.c *.h
