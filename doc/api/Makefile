#
#	Makefile -- API Documentation makefile
#
#	Copyright (c) Embedthis Software LLC, 2003-2011. All Rights Reserved.
#

include		.makedep

gen: prep mpr.html

prep:
	rm -f mpr.html

mpr.html: Makefile
	make -C mpr doc

tidy:
	tidy -q -config tidy.conf mpr.html

#
#   Local variables:
#   tab-width: 4
#   c-basic-offset: 4
#   End:
#   vim: sw=4 ts=4 noexpandtab
#
