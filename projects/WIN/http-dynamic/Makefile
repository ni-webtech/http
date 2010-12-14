#
#	Makefile for Windows Visual Studio projects
#
#	Copyright (c) Embedthis Software LLC, 2003-2009. All Rights Reserved.
#

include     .makedep

include     buildConfig.override

SRC			= ../../../src
CMD			= ../../../src/utils
HDRS	 	= $(BLD_INC_DIR)/buildConfig.h $(shell find $(SRC)/include -name '*.h')
LIBS		= ws2_32.lib
PROJECTS	= libmpr.vcproj libmprssl.vcproj libhttp.vcproj testHttp.vcproj http.vcproj
DFLAGS		= DEBUG_IDE

MAKE_IFLAGS	= $(BLD_MPR_IFLAGS) -I$(BLD_TOP)/src/include/master 
ifeq ($(BLD_FEATURE_OPENSSL),1)
	SSL_IFLAGS	+= $(BLD_OPENSSL_IFLAGS)
endif
ifeq ($(BLD_FEATURE_MATRIXSSL),1)
	SSL_IFLAGS	+= $(BLD_MATRIXSSL_IFLAGS)
endif

compileExtra: prep $(PROJECTS)

prep:
	if ls $(BLD_BIN_DIR)/*.def >/dev/null 2>&1 ; then \
		cp $(BLD_BIN_DIR)/*.def . ; \
	fi

libmpr.vcproj: Makefile buildConfig.override
	@makeVsProj --project libmpr.vcproj --library mpr --dflags "$(DFLAGS)" \
		--def libmpr.def --libs "$(LIBS)" $(HDRS) $(SRC)/deps/mpr/mprLib.c
	
libmprssl.vcproj: Makefile buildConfig.override
	@makeVsProj --project libmprssl.vcproj --library mprSsl --dflags "$(DFLAGS)" \
		--iflags "$(SSL_IFLAGS)" --def libmprssl.def --libs "libmpr.lib $(LIBS)" $(HDRS) $(SRC)/deps/mpr/mprSsl.c
	
libhttp.vcproj: Makefile buildConfig.override
	@makeVsProj --project libhttp.vcproj --library http --dflags "$(DFLAGS)" \
		--iflags "$(SSL_IFLAGS)" --def libmprssl.def --libs "libmpr.lib $(LIBS)" $(HDRS) \
	    $(shell find $(SRC)/ -name '*.c' | egrep -v 'deps/|cmd/|test/')
	
testHttp.vcproj: Makefile buildConfig.override
	@makeVsProj --project testHttp.vcproj --exe testHttp.exe --dflags "$(DFLAGS)" \
		--libs "$(LIBS) libhttp.lib libmpr.lib" $(HDRS) \
	    $(shell find $(SRC)/test -name '*.c' | egrep -v 'XXXX')

http.vcproj: Makefile buildConfig.override
	@makeVsProj --project http.vcproj --exe http.exe --dflags "$(DFLAGS)" \
		--libs "$(LIBS) libhttp.lib libmpr.lib" $(HDRS) $(SRC)/cmd/httpCmd.c 

cleanExtra:
	rm -rf Upgrade* _Upgrade*
