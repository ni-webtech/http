#
#	Makefile for Http tests
#
#	Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#
BUILD_DEPTH	?= 2

include 	.makedep

TARGETS		= $(BLD_BIN_DIR)/testHttp$(BLD_EXE)
OBJECTS		= $(patsubst %.c,$(BLD_OBJ_DIR)/%$(BLD_OBJ),$(wildcard test*.c))

compileTargets:	$(TARGETS)

$(BLD_BIN_DIR)/testHttp$(BLD_EXE): $(OBJECTS) $(BLD_LIB_DIR)/libmpr$(BLD_LIB)
	bld --exe $(BLD_BIN_DIR)/testHttp$(BLD_EXE) --search "$(BLD_MPR_LIBPATHS)" --libs "mpr $(BLD_SSL_LIBS) http" $(OBJECTS)

cleanExtra:
	rm -f trace*.txt *.txt.old


#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
