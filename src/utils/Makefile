#
#	Makefile for http utils
#
#	Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
#

include			.makedep

TARGETS			+= $(BLD_BIN_DIR)/http$(BLD_EXE)
MAKE_IFLAGS		+= $(BLD_MPR_IFLAGS)

compileExtra: $(TARGETS) 

$(BLD_BIN_DIR)/http$(BLD_EXE): $(BLD_OBJ_DIR)/http$(BLD_OBJ) $(BLD_LIB_DIR)/libhttp$(BLD_LIB)
	@bld --exe $(BLD_BIN_DIR)/http$(BLD_EXE) --search "$(BLD_SSL_LIBPATHS)" \
		--libs "http $(BLD_HTTP_WITHLIBS) $(BLD_MPR_LIBS) $(BLD_SSL_LIBS)" http

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
