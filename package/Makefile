#
#	Makefile for the all-in-one HTTP distribution
#
#	Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
#

STAGING			:= staging
DEST			:= $(STAGING)/src/deps/http
INC_DEST		:= $(STAGING)/src/include
DOC_DEST		:= $(STAGING)/doc
TARGETS			+= $(patsubst %,$(INC_DEST)/%, http.h)
TARGETS			+= $(patsubst %,$(DEST)/%, httpLib.c http.c)
ARCHIVE			+= $(BLD_TOP)/releases/http-all.tgz

include			.makedep

#
#	Order of headers matters
#
HEADERS			+= ../src/include/http.h
HTTP_SOURCES 	+= $(wildcard ../src/*.c) 

packageExtra: 	prep $(TARGETS) $(ARCHIVE)

prep:
	rm -fr $(STAGING)
	mkdir -p ../releases
	mkdir -p $(DEST) $(INC_DEST) $(DOC_DEST)/man $(DOC_DEST)/api

$(INC_DEST)/http.h: Makefile $(HEADERS)
	all-in-one $(HEADERS) | egrep -v '#include.*http|#inc.*config.h|#inc.*ucp.*\.h|#inc.*pcre' >$(INC_DEST)/http.h

$(DEST)/httpLib.c: Makefile $(HEADERS) $(HTTP_SOURCES)
	echo '#include "http.h"' >$(DEST)/httpLib.c
	all-in-one $(HTTP_SOURCES) | egrep -v '#inc.*http' >>$(DEST)/httpLib.c

$(DEST)/http.c: Makefile ../src/utils/httpCmd.c
	cp ../src/utils/httpCmd.c $(DEST)/http.c

$(ARCHIVE): Makefile $(TARGETS)
	cp $(BLD_TOP)/doc/api/httpBare.html $(STAGING)/doc/api
	cp $(BLD_TOP)/doc/man/*.1 $(STAGING)/doc/man
	cp Makefile.all $(DEST)/Makefile
	@$(call log) "[Generate]" "tar cfz $(ARCHIVE) -C $(STAGING) ."
	tar cfz $(ARCHIVE) -C $(STAGING) .
	rm -fr $(STAGING)

cleanExtra:
	rm -rf $(STAGING)
