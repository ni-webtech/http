#
#	Makefile for http library source
#
#	Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

include			.makedep

PRE_DIRS		+= deps stages
POST_DIRS		+= utils test

TARGET			+= $(BLD_LIB_DIR)/libhttp$(BLD_LIB)

#
#	Build the targets in this directory first then the builtin packages (ssl and regexp)
#
compileExtra: $(TARGET) 

$(TARGET): $(OBJECTS)
	bld --library $(BLD_LIB_DIR)/libhttp --search "$(BLD_MPR_LIBPATHS) $(BLD_SSL_LIBPATHS)" \
		--libs "$(BLD_SSL_LIBS) $(BLD_MPR_LIBS)" $(OBJECTS)

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
