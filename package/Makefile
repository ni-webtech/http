#
#	Makefile for the dist and flat HTTP distributions
#
#	Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
#

include			.makedep

STAGING			:= $(PWD)/staging
TDIR			:= $(STAGING)/http-$(BLD_VERSION)
REL_DIR			:= ../releases
DIST_VER_NAME	:= http-$(BLD_VERSION)-dist.tgz
DIST_PERM_NAME	:= http-dist.tgz
DIST_VER		:= $(REL_DIR)/$(DIST_VER_NAME)
DIST_PERM		:= $(REL_DIR)/$(DIST_PERM_NAME)
FLAT_VER_NAME	:= http-$(BLD_VERSION)-flat.tgz
FLAT_PERM_NAME	:= http-flat.tgz
FLAT_VER		:= $(REL_DIR)/$(FLAT_VER_NAME)
FLAT_PERM		:= $(REL_DIR)/$(FLAT_PERM_NAME)
DEST			:= $(TDIR)/src/deps/http
INC_DEST		:= $(TDIR)/src/include
DOC_DEST		:= $(TDIR)/doc
TARGETS			+= $(patsubst %,$(INC_DEST)/%, http.h)
TARGETS			+= $(patsubst %,$(DEST)/%, httpLib.c http.c)
SRC				:= ../src

#
#	Order of headers matters
#
HEADERS			+= ../src/include/http.h
HTTP_SOURCES 	+= $(wildcard ../src/*.c) 

dist packageExtra: prep $(TARGETS) $(DIST_VER) $(FLAT_VER) cleanup

prep:
	rm -fr $(STAGING) $(DIST_VER) $(DIST_PERM) $(FLAT_VER) $(FLAT_PERM)
	mkdir -p $(REL_DIR)
	mkdir -p $(DEST) $(INC_DEST) $(DOC_DEST)/man $(DOC_DEST)/api

$(INC_DEST)/http.h: Makefile $(HEADERS)
	dist $(HEADERS) | egrep -v '#include.*http|#inc.*config.h|#inc.*ucp.*\.h|#inc.*pcre' >$(INC_DEST)/http.h

$(DEST)/httpLib.c: Makefile $(HEADERS) $(HTTP_SOURCES)
	echo '#include "http.h"' >$(DEST)/httpLib.c
	dist $(HTTP_SOURCES) | egrep -v '#inc.*http' >>$(DEST)/httpLib.c

$(DEST)/http.c: Makefile ../src/utils/httpCmd.c
	cp ../src/utils/httpCmd.c $(DEST)/http.c

$(DIST_VER): Makefile $(TARGETS)
	cp $(BLD_TOP)/doc/api/httpBare.html $(TDIR)/doc/api
	cp $(BLD_TOP)/doc/man/*.1 $(TDIR)/doc/man
	cp Makefile.dist $(DEST)/Makefile
	@$(call log) "[Generate]" "tar cfz $(DIST_VER) -C $(STAGING) ."
	tar cfz $(DIST_VER) -C $(STAGING) .
	cd $(REL_DIR) ; ln -s $(DIST_VER_NAME) $(DIST_PERM_NAME)

flat $(FLAT_VER):
	find $(TDIR)/src -type f | xargs -I % mv % $(TDIR) 
	cp $(SRC)/deps/mpr/mprLib.c $(SRC)/deps/mpr/mprSsl.c $(TDIR)
	cp $(SRC)/include/mpr.h $(SRC)/include/mprSsl.h $(TDIR) 
	rm -fr $(TDIR)/doc $(TDIR)/src $(TDIR)/Makefile
	$(call log) "[Generate]" "tar cfz $(FLAT_VER) -C $(STAGING) ."
	tar cfz $(FLAT_VER) -C $(STAGING) .
	cd $(REL_DIR) ; ln -s $(FLAT_VER_NAME) $(FLAT_PERM_NAME)

cleanup:
	rm -rf $(STAGING)

cleanExtra:
	rm -rf $(STAGING) $(REL_DIR)/*.tgz
