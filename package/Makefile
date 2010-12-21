#
#	Makefile for the Http distributions
#
#	Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

include			.makedep

STAGE_DIR			:= $(BLD_OUT_DIR)/staging
TDIR			:= $(STAGE_DIR)/http-$(BLD_VERSION)
REL_DIR			:= $(BLD_OUT_DIR)/releases
DIST_VER_NAME	:= http-$(BLD_VERSION)-dist.tgz
DIST_PERM_NAME	:= http-dist.tgz
FLAT_VER_NAME	:= http-$(BLD_VERSION)-flat.tgz
FLAT_PERM_NAME	:= http-flat.tgz
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

dist packageExtra: clean prep $(TARGETS) $(REL_DIR)/$(DIST_VER_NAME) $(REL_DIR)/$(FLAT_VER_NAME) cleanup

prep:
	mkdir -p $(STAGE_DIR) $(REL_DIR) $(DEST) $(INC_DEST) $(DOC_DEST)/man $(DOC_DEST)/api

$(INC_DEST)/http.h: Makefile $(HEADERS)
	TITLE="$(BLD_NAME) Header" \
	dist $(HEADERS) | egrep -v '#include.*http|#inc.*config.h|#inc.*ucp.*\.h|#inc.*pcre' >$(INC_DEST)/http.h

$(DEST)/httpLib.c: Makefile $(HEADERS) $(HTTP_SOURCES)
	echo '#include "http.h"' >$(DEST)/httpLib.c
	TITLE="$(BLD_NAME) Library Source" \
	dist $(HTTP_SOURCES) | egrep -v '#inc.*http' >>$(DEST)/httpLib.c

$(DEST)/http.c: Makefile ../src/utils/httpCmd.c
	cp ../src/utils/httpCmd.c $(DEST)/http.c

$(REL_DIR)/$(DIST_VER_NAME): Makefile $(TARGETS)
	cp $(BLD_TOP)/doc/api/httpBare.html $(TDIR)/doc/api
	cp $(BLD_TOP)/doc/man/*.1 $(TDIR)/doc/man
	cp Makefile.dist $(DEST)/Makefile
	@$(call log) "[Generate]" "tar cfz $(REL_DIR)/$(DIST_VER_NAME) -C $(STAGE_DIR) ."
	tar cfz $(REL_DIR)/$(DIST_VER_NAME) -C $(STAGE_DIR) .
	cd $(REL_DIR) ; ln -s $(DIST_VER_NAME) $(DIST_PERM_NAME)

flat $(REL_DIR)/$(FLAT_VER_NAME):
	find $(TDIR)/src -type f | xargs -I % mv % $(TDIR) 
	cp $(SRC)/deps/mpr/mprLib.c $(SRC)/deps/mpr/mprSsl.c $(TDIR)
	cp $(SRC)/include/mpr.h $(SRC)/include/mprSsl.h $(TDIR) 
	rm -fr $(TDIR)/doc $(TDIR)/src $(TDIR)/Makefile
	$(call log) "[Generate]" "tar cfz $(REL_DIR)/$(FLAT_VER_NAME) -C $(STAGE_DIR) ."
	tar cfz $(REL_DIR)/$(FLAT_VER_NAME) -C $(STAGE_DIR) .
	ln -s $(FLAT_VER_NAME) $(REL_DIR)/$(FLAT_PERM_NAME)

cleanup:
	rm -rf $(STAGE_DIR)

cleanExtra:
	rm -rf $(STAGE_DIR) $(REL_DIR)/$(DIST_VER_NAME) $(REL_DIR)/$(DIST_PERM_NAME)
	rm -fr $(REL_DIR)/$(FLAT_VER_NAME) $(REL_DIR)/$(FLAT_PERM_NAME)
