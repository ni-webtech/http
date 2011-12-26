#
#	Makefile -- API Documentation makefile
#
#	Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.
#

include		.makedep

gen: prep http.html

prep:
	touch Makefile

http.html: Makefile
	make -C http gen

tidy:
	tidy -q -config tidy.conf http.html

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
