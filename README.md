printfq
=======

A utility for formatting text to be processed as arguments by a shell.
This performs the same basic function as the %q format option found in several
printf utilities, hence its name.  This goes a bit further by identifying
Unicode code points while being tolerant of invalid UTF-8 encoding, by
producing shortened escape sequences where reasonable, by stream processing,
and by providing an option to produce output that is compatible with a bare
POSIX shell such as dash.  See the command synopsis below.
  
The impetus for this was a scenario where escaping was needed for an ash shell
on an embedded system.  I threw together a quick and dirty solution that
quoted all arguments, although I could easily imagine something better.  A
secondary intention was to eliminate the printf dependency in
[jkparse](https://github.com/jacre8/jkparse).  Like so many projects, this
took on a life of its own.


Command Synopsis
----------------

	  printfq: Escape strings for input processing by a POSIX compatible shell.
	Input can come from one or more arguments or, in the absence of non-option
	arguments, from stdin.  Each non-option argument or null terminated string
	from stdin is, by default, individually escaped and separated by a space
	character from other arguments/strings in the output.  In the absence of any
	option arguments, this produces formatting that is compatible with bash,
	busybox sh, ksh, and zsh.
	  The LANG environment variable determines both the input and the output
	character encoding.  Regardless of the locale, however, non-printable code
	points will, by default, be output as escaped bytes of their UTF-8 encoding.
	Piping the output through `iconv -t UTF-8` should produce output that is
	suitable for processing as UTF-8.  The `locale -c charmap` command can be used
	to check what encoding a particular locale uses

	OPTIONS:
	 -e, --escape-more
	    Escape Unicode code points other than the ASCII space character (0x20)
	  that, by themselves, have no glyph.  This includes other space characters and
	  all characters that are escaped with --escape-invisible.  This option does
	  not guarantee that all unescaped characters will render.  The --minimal
	  option supercedes this option
	 -f, --flush-arguments
	    Flush the output buffer between input strings, and delimit output using
	  null characters as though --null-terminated-output is specified.  This
	  option is intended to facilitate running this as a coprocess
	 -i, --escape-invisible
	    Escape Unicode code points that are invisible by themselves, in addition to
	  those identified as non-printable by iswprint().  This includes contextual
	  code points such as zero width spaces, but not other space characters.  This
	  option's implementation is not exhaustive and cannot guarantee that unescaped
	  characters will render.  The --minimal option supercedes this option
	 -m, --minimal
	    Do not use ANSI-C style quoting ($'') or its escapes for non-printable
	  characters.  This will produce machine readable output that can be processed
	  by most shells, including a strictly POSIX conforming shell such as dash...
	  at least in a C or UTF-8 encoded locale
	 -n, --ignore-null-input
	    Ignore null characters read over stdin and treat all streamed input as a
	  single string.  This option has no effect when there are non-option arguments
	 -u, --unicode-escapes
	    Escape non-printable, yet valid, Unicode code points that are greater than
	  127 using $'\uXXXX' or $'\UXXXXXXXX' syntax, instead of escaping individual
	  bytes of their UTF-8 encoding.  Additionally, escape the escape character
	  using $'\E' rather than its numeric value, $'\033'.  In a UTF-8 encoded
	  locale, improperly encoded bytes from the input are still individually
	  escaped in the output.  This produces shorter and more human readable output
	  but breaks compatibility with busybox sh.  This option does nothing in the C
	  locale or if --minimal is also specified
	 -z, --null-terminated-output
	    Instead of using space characters to delimit arguments/strings, terminate
	  all arguments/strings in the output, including the last, with null characters
	 --
	    End of input.  Use this to protect input arguments from option processing
	 --help
	    This output
	 --version
	    Version information

Building and Installing
-----------------------

The C library is the only dependency.  This has only been tested with gcc and
glibc.  To build and install:  

	make
	sudo make install