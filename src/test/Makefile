#
#	Makefile for Http tests
#
#	Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
#

BUILD_DEPTH		?= 2

include 		.makedep

MAKE_IFLAGS		= $(BLD_MPR_IFLAGS)
TARGETS			+= $(BLD_BIN_DIR)/testHttp$(BLD_EXE)
OBJECTS			= $(patsubst %.c,$(BLD_OBJ_DIR)/%$(BLD_OBJ),$(wildcard test*.c))

compileTargets:	$(TARGETS)

$(BLD_BIN_DIR)/testHttp$(BLD_EXE): $(OBJECTS)
	bld --exe $(BLD_BIN_DIR)/testHttp$(BLD_EXE) --search "$(BLD_SSL_LIBPATHS)" \
		--libs "$(BLD_HTTP_WITHLIBS) $(BLD_MPR_LIBS) $(BLD_SSL_LIBS) http" $(OBJECTS)

cleanExtra:
	rm -f trace*.txt *.txt.old

ifeq	($(BLD_CROSS),0)
testExtra::
	testHttp -v
endif

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
