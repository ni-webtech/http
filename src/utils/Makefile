#
#	Makefile for http utils
#
#	Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

include			.makedep

TARGETS			+= $(BLD_BIN_DIR)/http$(BLD_EXE)

compileExtra: $(TARGETS) 

$(BLD_BIN_DIR)/http$(BLD_EXE): $(BLD_OBJ_DIR)/http$(BLD_OBJ) $(BLD_LIB_DIR)/libhttp$(BLD_LIB)
	@bld --exe $(BLD_BIN_DIR)/http$(BLD_EXE) --search "$(BLD_MPR_LIBPATHS) $(BLD_SSL_LIBPATHS)" \
		--libs "http $(BLD_MPR_LIBS) $(BLD_SSL_LIBS)" http

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
