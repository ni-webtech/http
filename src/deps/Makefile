#
#	Makefile for http dependencies
#
#	Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
#

include			.makedep

ifeq ($(BLD_PCRE_BUILTIN),1)
	PRE_DIRS    += pcre
endif
ifeq ($(BLD_MPR_BUILTIN),1)
	PRE_DIRS    += mpr
else
	IMPORTS     += $(BLD_SSL_IMPORTS)
endif

IMPORTED    	+= $(patsubst %,$(BLD_LIB_DIR)/%, $(notdir $(IMPORTS)))

compileFirst:	$(IMPORTED)

$(IMPORTED): $(IMPORTS)
	getlib $^

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
