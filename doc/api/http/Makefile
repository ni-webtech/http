#
#	Makefile -- MPR API Documentation
#
#	Copyright (c) Embedthis Software LLC, 2003-2010. All Rights Reserved.
#

NAME		= http

include		.makedep

doc:
	rm ../../../all/*.c
	make -C ../../../all
	rm -fr xml html
	mkdir -p xml html
	doxygen $(NAME).spec
	ejs $(BLD_TOOLS_DIR)/gendoc.es --bare --title "Mpr - Portable Runtime Native API" --tags ../$(NAME).tags \
		xml/$(NAME).xml xml/group* xml/struct_*.xml >../$(NAME)Bare.html
	ejs $(BLD_TOOLS_DIR)/gendoc.es --title "Mpr - Portable Runtime Native API" --tags ../$(NAME).tags \
		xml/$(NAME)_8h.xml xml/group* xml/struct_*.xml >../$(NAME).html

x:
	ejs $(BLD_TOOLS_DIR)/gendoc.es --title "Mpr - Portable Runtime Native API" --tags ../$(NAME).tags \
		xml/$(NAME)_8h.xml xml/group* xml/struct_*.xml >../$(NAME).html

cleanExtra:
	rm -rf xml html ../$(NAME).tags

clobberExtra:
	rm -f ../$(NAME).html ../$(NAME)Bare.html
