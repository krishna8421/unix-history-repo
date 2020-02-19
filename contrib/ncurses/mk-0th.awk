# $Id: mk-0th.awk,v 1.23 2020/02/02 23:34:34 tom Exp $
##############################################################################
# Copyright 2020 Thomas E. Dickey                                            #
# Copyright 1998-2010,2012 Free Software Foundation, Inc.                    #
#                                                                            #
# Permission is hereby granted, free of charge, to any person obtaining a    #
# copy of this software and associated documentation files (the "Software"), #
# to deal in the Software without restriction, including without limitation  #
# the rights to use, copy, modify, merge, publish, distribute, distribute    #
# with modifications, sublicense, and/or sell copies of the Software, and to #
# permit persons to whom the Software is furnished to do so, subject to the  #
# following conditions:                                                      #
#                                                                            #
# The above copyright notice and this permission notice shall be included in #
# all copies or substantial portions of the Software.                        #
#                                                                            #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    #
# THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        #
# DEALINGS IN THE SOFTWARE.                                                  #
#                                                                            #
# Except as contained in this notice, the name(s) of the above copyright     #
# holders shall not be used in advertising or otherwise to promote the sale, #
# use or other dealings in this Software without prior written               #
# authorization.                                                             #
##############################################################################
#
# Author: Thomas E. Dickey 1996-on
#
# Generate list of sources for a library, together with lint/lintlib rules
#
# Variables:
#	libname (library name, e.g., "ncurses", "panel", "forms", "menus")
#	subsets (is used here to decide if wide-character code is used)
#	ticlib (library name for libtic, e.g., "tic")
#	termlib (library name for libtinfo, e.g., "tinfo")
#
function make_lintlib(name,sources) {
	print  ""
	print  "clean ::"
	printf "\trm -f llib-l%s.*\n", name
	print  ""
	print  "realclean ::"
	printf "\trm -f llib-l%s\n", name
	print  ""
	printf "llib-l%s : %s\n", name, sources
	printf "\tcproto -a -l -DNCURSES_ENABLE_STDBOOL_H=0 -DLINT $(CPPFLAGS) %s >$@\n", sources
	print  ""
	print  "lintlib ::"
	printf "\tsh $(srcdir)/../misc/makellib %s $(CPPFLAGS)\n", name
	print ""
	print "lint ::"
	printf "\t$(LINT) $(LINT_OPTS) $(CPPFLAGS) %s $(LINT_LIBS)\n", sources
}

# A blank in "subsets" indicates a split-off of the library into a separate
# file, e.g., for libtic or libtinfo.  They are all logical parts of the same
# library.
function which_library() {
	if ( ( which == "ticlib" ) && ( subsets ~ /ticlib / ) ) {
		return ticlib;
	} else if ( ( which == "termlib" || which == "ext_tinfo" ) && ( subsets ~ /[[:space:]]base/ ) ) {
		return termlib;
	} else {
		return libname;
	}
}

function show_list(name, len, list) {
	if ( len > 0 ) {
		printf "\n%s_SRC =", toupper(name);
		for (n = 0; n < len; ++n)
			printf " \\\n\t%s", list[n];
		print "";
		make_lintlib(name, sprintf("$(%s_SRC)", toupper(name)));
	}
}

BEGIN	{
		which = libname;
		using = 0;
		found = 0;
		count_ticlib = 0;
		count_termlib = 0;
		count_library = 0;
	}
	/^@/ {
		which = $0;
		sub(/^@[[:blank:]]+/, "", which);
		sub(/[[:blank:]]+$/, "", which);
	}
	!/^[@#]/ {
		if (using == 0)
		{
			print  ""
			print  "# generated by mk-0th.awk"
			printf "#   libname:    %s\n", libname
			printf "#   subsets:    %s\n", subsets
			if ( libname ~ /ncurses/ ) {
				printf "#   ticlib:     %s\n", ticlib
				printf "#   termlib:    %s\n", termlib
			}
			print  ""
			print  ".SUFFIXES: .c .cc .h .i .ii"
			print  ".c.i :"
			printf "\t$(CPP) $(CPPFLAGS) $< >$@\n"
			print  ".cc.ii :"
			printf "\t$(CPP) $(CPPFLAGS) $< >$@\n"
			print  ".h.i :"
			printf "\t$(CPP) $(CPPFLAGS) $< >$@\n"
			print  ""
			using = 1;
		}
		if (which ~ /port_/ )
		{
			# skip win32 source
		}
		else if ( $0 != "" && $1 != "link_test" )
		{
			if ( found == 0 )
			{
				if ( subsets ~ /widechar/ )
					widechar = 1;
				else
					widechar = 0;
				printf "C_SRC ="
				if ( $2 == "lib" )
					found = 1
				else
					found = 2
			}
			if ( libname == "c++" || libname == "c++w" ) {
				srcname = sprintf("%s/%s.cc", $3, $1);
				printf " \\\n\t%s", srcname;
			} else if ( widechar == 1 || $3 != "$(wide)" ) {
				srcname = sprintf("%s/%s.c", $3, $1);
				printf " \\\n\t%s", srcname;
				if ( which_library() == libname ) {
					list_library[count_library++] = srcname;
				} else if ( which_library() == ticlib ) {
					list_ticlib[count_ticlib++] = srcname;
				} else {
					list_termlib[count_termlib++] = srcname;
				}
			}
		}
	}
END	{
		print  ""
		if ( found == 1 )
		{
			print  ""
			printf "# Producing llib-l%s is time-consuming, so there's no direct-dependency for\n", libname;
			print  "# it in the lintlib rule.  We'll only remove in the cleanest setup.";
			show_list(libname, count_library, list_library);
			show_list(ticlib, count_ticlib, list_ticlib);
			show_list(termlib, count_termlib, list_termlib);
		}
		else
		{
			print  ""
			print  "lintlib :"
			print  "\t@echo no action needed"
		}
	}
# vile:ts=4 sw=4
