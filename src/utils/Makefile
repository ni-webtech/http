#
#	Makefile for http utils
#
#	Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
#

include			.makedep

TARGETS			+= $(BLD_BIN_DIR)/http$(BLD_EXE)

compileExtra: $(TARGETS) 

$(BLD_BIN_DIR)/http$(BLD_EXE): $(BLD_OBJ_DIR)/httpCmd$(BLD_OBJ) $(BLD_LIB_DIR)/libhttp$(BLD_LIB)
	@bld --exe $(BLD_BIN_DIR)/http$(BLD_EXE) --search "$(BLD_MPR_LIBPATHS)" \
		--libs "http $(BLD_MPR_LIBS) $(BLD_SSL_LIBS)" httpCmd

