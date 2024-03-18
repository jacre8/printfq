//  printfq
//  Escape one or more strings for input processing by a POSIX shell.
// This performs the same basic function as the %q format specifier found in some versions of
// printf.  Multiple strings may be provided as arguments or, if there are no arguments, via
// stdin.

#define PRINTFQ_VERSION_STRING "3"

#define PRINTFQ_VERSION_STRING_LONG "printfq version " PRINTFQ_VERSION_STRING \
"\nCopyright (C) 2024 Jason Hinsch\n" \
"License: GPLv2 <https://www.gnu.org/licenses/old-licenses/gpl-2.0.html>\n" \
"See https://github.com/jacre8/printfq for the latest version and documentation"

//  As written, it is assumed that wchar_t stores a Unicode code point for \u and \U output
// with wide characters, and inside iswprintExt() and iswNotBlank().
#ifndef __STDC_ISO_10646__
#warning The Unicode code point mapping has not been tested in this configuration!
#endif

const char versionString[] = "printfq version 3";

#define _GNU_SOURCE
#include <ctype.h>  // isprint()
#include <getopt.h>
#include <errno.h>
#include <langinfo.h> //nl_langinfo
#include <locale.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h> // iswprint()

//  In addition to those characters identified by iswprint() as non-printable, this function
// identifies unicode characters that are invisible by themselves, including 0-space characters.
// This is a subset of the list at https://invisible-characters.com/.  Space characters from that
// list with a non-zero width have been omitted here, but appear in iswNotBlank().
static int iswprintExt(wint_t c)
{
	return iswprint(c) &&
	//  None of the uncommented are excluded by iswprint() in glibc v2.28
	//  0x9, 0x20 and 0xA0 are non-zero spaces
	// 0xAD renders in my terminal as a non-zero space, although it shouldn't
	0xAD != c &&
	0x034F != c &&
	0x061C != c &&
	0x115F != c &&
	0x1160 != c &&
	0x17B4 != c &&
	0x17B5 != c &&
	(0x180B > c || 0x180E < c) &&
	//  0x2000 - 0x200A are non-zero spaces
	(0x200B > c || 0x200F < c) &&
	(0x202A > c || 0x202E < c) &&
	//  0x202F and 0x205F are non-zero spaces
	(0x2060 > c || 0x206F < c) &&
	//  0x2800, 0x3000, and 0x3164 are non-zero spaces
	(0xFE00 > c || 0xFE0F < c) &&
	0xFEFF != c &&
	0xFFA0 != c &&
	//  0xFFFC renders as a non-zero space for me, but it shouldn't
	0xFFFC != c &&
	// 0x133FC renders as a non-zero space but is, by definition, printable
	// 0x1D159 renders as a non-zero space for me, but it shouldn't
	0x1D159 != c &&
	(0x1D173 > c || 0x1D17A < c) &&
	0xE0001 != c &&
	(0xE0020 > c || 0xE007F < c) &&
	(0xE0100 > c || 0xE01EF < c);
}

//  This will return true if the character is graphic or is a regular space.
static int iswNotBlank(wint_t c)
{
	//  0x9 and 0x20 are recognized by iswspace().  That aside, 0x20 will not be escaped
	// (iswprintExt() returns true for it) and iswprintExt() returns false for all other whitespace
	// characters below 128.  Futhermore, the control characters in the 0x80-0x9F block are
	// caught by iswprint(), and the only other non-graphic characters below 256 are 0xAD and
	// 0xA0 which are both explicitly checked for.
	return iswprintExt(c) && (0x100 > c ? 0xA0 != c : ! (
		iswspace(c) ||
		//  None of the uncommented are caught by iswspace() in glibc v2.28
		//0xA0 == c ||
		// 0x2000 - 0x2006 are caught by iswspace(), as are 0x2008-0x200A, but not 0x2007
		//(0x2000 <= c && 0x200A >= c) ||
		0x2007 == c ||
		0x202F == c ||
		//0x205F == c || // caught by iswspace()
		0x2800 == c ||
		//0x3000 == c || // caught by iswspace()
		0x3164 == c
	));
}

static char stdoutBuffer[BUFSIZ];
static char stdinBuffer[BUFSIZ];

int main(int argc, char **argv)
{
	setvbuf(stdout, stdoutBuffer, _IOFBF, sizeof(stdoutBuffer));
	setvbuf(stdin, stdinBuffer, _IOFBF, sizeof(stdinBuffer));
	//  These are characters that must always be escaped or quoted
	// to avoid interpretation by the shell.  See
	// https://pubs.opengroup.org/onlinepubs/9699919799/utilities/V3_chap02.html#tag_18_02
	// = and % have been omitted here since, at least as an argument, it does not appear to be
	// possible to mis-interpret them.  The tilde (~) has specific handling.  There is room
	// for improvement with the other contextual escapes: *, ?, [, and #.
	// ^ is escaped in case the escaped string is placed inside a bracket expansion (bash
	// recognizes it).  Perhaps an argument indicating that the output will not be used as a
	// test argument would make sense?  
	static const char shControlChars[] = {
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 1, 1, 0, 0, 0, 0, 0,	// tab, newline
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		1, 1, 1, 1, 1, 0, 1, 1, // space ! " # $ & '
		1, 1, 1, 0, 1, 0, 0, 0, // ( ) * comma
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 1, 1, 0, 1, 1, // ; < > ?

		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 1, 1, 1, 1, 0, // [ \ ] ^
		1, 0, 0, 0, 0, 0, 0, 0, // `
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 1, 1, 1, 0, 0  // { | }
	};
	//  These are non-printable characters that have defined escapes inside $'' quoting,
	// and whose escapes are the indicated letters.
	static const char ansiEscapes[] = {
		0,   0,   0,   0,   0,   0,   0,   'a', // bell
		// backspace, tab, newline, vertical tab, form feed, carriage return
		'b', 't', 'n', 'v', 'f', 'r', 0,   0,
		0,   0,   0,   0,   0,   0,   0,   0,
		0,   0,   0, 'E'  // escape
	};
	//  The \E escape for the escape character (0x1B) is recognized by bash, ksh, and zsh,
	// but it is not recognized by busybox sh.  This limit will be increased when the -u
	// option is specified.
	unsigned ansiEscapesLimit = 14;
	int (* iswprintFn)(wint_t c) = iswprint;
	unsigned disableCQuoting = 0;
	unsigned useUnicodeEscapes = 0;
	unsigned flushArguments = 0;
	unsigned ignoreNullInput = 0;
	unsigned nullTerminatedOutput = 0;
	{
		int currentoption; // for getopt parsing
		// Long option parsing vars
		static const struct option longopts[] = {
			// {.name, .has_arg, .flag, .val}
			{"help", no_argument, NULL, '$'},
			{"escape-more", no_argument, NULL, 'e'},
			{"flush-arguments", no_argument, NULL, 'f'},
			{"escape-invisible", no_argument, NULL, 'i'},
			{"minimal", no_argument, NULL, 'm'},
			{"ignore-null-input", no_argument, NULL, 'n'},
			{"unicode-escapes", no_argument, NULL, 'u'},
			{"null-terminated-output", no_argument, NULL, 'z'},
			{"version", no_argument, NULL, '%'},
			{0, 0, 0, 0}
		};
		while(-1 != (currentoption = getopt_long(argc, argv, ":efimnuz", longopts, &currentoption)))
		{
			switch(currentoption) {
			case '$':
				puts("  printfq: Escape strings for input processing by a POSIX compatible shell.\n"
					"Input can come from one or more arguments or, in the absence of non-option\n"
					"arguments, from stdin.  Each non-option argument or null terminated string\n"
					"from stdin is, by default, individually escaped and separated by a space\n"
					"character from other arguments/strings in the output.  In the absence of any\n"
					"option arguments, this produces formatting that is compatible with bash,\n"
					"busybox sh, ksh, and zsh.\n"
					"  The LANG environment variable determines both the input and the output\n"
					"character encoding.  Regardless of the locale, however, non-printable code\n"
					"points will, by default, be output as escaped bytes of their UTF-8 encoding.\n"
					"Piping the output through `iconv -t UTF-8` should produce output that is\n"
					"suitable for processing as UTF-8.  The `locale -c charmap` command can be used\n"
					"to check what encoding a particular locale uses\n\n"
					"OPTIONS:\n"
					" -e, --escape-more\n"
					"    Escape Unicode code points other than the ASCII space character (0x20)\n"
					"  that, by themselves, have no glyph.  This includes other space characters and\n"
					"  all characters that are escaped with --escape-invisible.  This option does\n"
					"  not guarantee that all unescaped characters will render.  The --minimal\n"
					"  option supercedes this option\n"
					" -f, --flush-arguments\n"
					"    Flush the output buffer between input strings, and delimit output using\n"
					"  null characters as though --null-terminated-output is specified.  This\n"
					"  option is intended to facilitate running this as a coprocess\n"
					" -i, --escape-invisible\n"
					"    Escape Unicode code points that are invisible by themselves, in addition to\n"
					"  those identified as non-printable by iswprint().  This includes contextual\n"
					"  code points such as zero width spaces, but not other space characters.  This\n"
					"  option's implementation is not exhaustive and cannot guarantee that unescaped\n"
					"  characters will render.  The --minimal option supercedes this option\n"
					" -m, --minimal\n"
					"    Do not use ANSI-C style quoting ($'') or its escapes for non-printable\n"
					"  characters.  This will produce machine readable output that can be processed\n"
					"  by most shells, including a strictly POSIX conforming shell such as dash...\n"
					"  at least in a C or UTF-8 encoded locale\n"
					" -n, --ignore-null-input\n"
					"    Ignore null characters read over stdin and treat all streamed input as a\n"
					"  single string.  This option has no effect when there are non-option arguments\n"
					//" -o, --optimize-output-length\n"
					//"    Attempt to produce shorter output by processing the input multiple times\n"
					" -u, --unicode-escapes\n"
					"    Escape non-printable, yet valid, Unicode code points that are greater than\n"
					"  127 using $'\\uXXXX' or $'\\UXXXXXXXX' syntax, instead of escaping individual\n"
					"  bytes of their UTF-8 encoding.  Additionally, escape the escape character\n"
					"  using $'\\E' rather than its numeric value, $'\\033'.  In a UTF-8 encoded\n"
					"  locale, improperly encoded bytes from the input are still individually\n"
					"  escaped in the output.  This produces shorter and more human readable output\n"
					"  but breaks compatibility with busybox sh.  This option does nothing in the C\n"
					"  locale or if --minimal is also specified\n"
					" -z, --null-terminated-output\n"
					"    Instead of using space characters to delimit output arguments, delimit\n"
					"  output arguments with null characters.  The last output argument will also be\n"
					"  null terminated if it is terminated in input, if --ignore-null-input is\n"
					"  specified, or if the input comes from non-option arguments\n"
					" --\n"
					"    End of input.  Use this to protect input arguments from option processing\n"
					" --help\n"
					"    This output\n"
					" --version\n"
					"    Version information"
				#ifndef __STDC_ISO_10646__
					"\n\nBUGS:  Unicode code points may not be properly mapped in this build."
				#endif
				);
				exit(0);
				break;
			case 'e':
				iswprintFn = iswNotBlank;
				break;
			case 'f':
				flushArguments = nullTerminatedOutput = 1;
				break;
			case 'i':
				if(iswNotBlank != iswprintFn)
					iswprintFn = iswprintExt;
				break;
			case 'm':
				disableCQuoting = 1;
				break;
			case 'n':
				ignoreNullInput = 1;
				break;
			case 'u':
				useUnicodeEscapes = 1;
				ansiEscapesLimit = sizeof(ansiEscapes);
				break;
			case 'z':
				nullTerminatedOutput = 1;
				break;
			case '%':
				puts(PRINTFQ_VERSION_STRING_LONG);
				exit(0);
				break;
			case '?':
				if(0 == optopt)
					fprintf(stderr, "Invalid option: %s\n", argv[optind-1]);
				else
					fprintf(stderr, "Invalid option: -%c\n", optopt);
				#if defined(__GNUC__) && __GNUC__ >= 7
					__attribute__((fallthrough));
				#endif
			default:
				return EX_USAGE;
			}
		}
	}
	if(optind < argc) {
		//  Fork and use the stream processing implementation for the arguments
		ignoreNullInput = 0;
		int streamPipe[2];
		pid_t streamPid;
		if(pipe(streamPipe) || -1 == (streamPid = fork()))
			return EX_OSERR;
		if(streamPid) {
			argv = argv + optind;
			if(*(argv + 1)) {
				FILE * stream;
				if(! (stream = fdopen(streamPipe[1], "w")))
					return EX_IOERR;
				close(streamPipe[0]);
				static char streamBuffer[BUFSIZ];
				setvbuf(stream, streamBuffer, _IOFBF, sizeof(streamBuffer));
				do {
					fputs_unlocked(*argv, stream);
					if(EOF == putc_unlocked(0, stream))
						return EX_IOERR;
				} while(*(++argv));
				fclose(stream);
			}
			else {
				int streamFd = streamPipe[1];
				//do {
					char * str = *argv;
					size_t size = strlen(str) + 1;
					ssize_t writeRc;
					do {
						if(0 > (writeRc = write(streamFd, str, size)))
							return EX_IOERR;
						str += writeRc;
					} while((size -= writeRc));
				//} while(*(++argv));
				close(streamFd);
			}
			int status;
			waitpid(streamPid, &status, 0);
			return WIFEXITED(status) ? WEXITSTATUS(status) : status + 128;
		}
		else {
			close(streamPipe[1]);
			dup2(streamPipe[0], 0);
		}
	}
	unsigned localeIsNotUtf8;
	if(! setlocale(LC_ALL, "") || ({
		char * currentLocale = nl_langinfo(CODESET);
		//  The ASCII handling is also used with a UTF-8 locale when not escaping
		// non-printable characters since it is functionally equivalent in that case
		// and avoids additional conditionals in the UTF-8 handling.
		(! (localeIsNotUtf8 = strcmp("UTF-8", currentLocale)) && disableCQuoting) ||
		(localeIsNotUtf8 && ! strcmp("ANSI_X3.4-1968", currentLocale));
	})) {
		int c = getc_unlocked(stdin);
		unsigned isPrintable;
		if('~' == c) {
			isPrintable = 1;
			goto narrowCharStartEscape;
		}
		do {
			if(0 < c) {
				do {
					isPrintable = disableCQuoting || isprint(c);
					if(((unsigned)c < sizeof(shControlChars) && shControlChars[c]) || ! isPrintable) {
						narrowCharStartEscape:
						if('\'' == c || ({
							if(disableCQuoting) {
								putc_unlocked('\'', stdout);
								do
									putc_unlocked(c, stdout);
								while(0 < (c = getc_unlocked(stdin)) && '\'' != c);
							}
							else {
								fputs_unlocked("$'", stdout);
								do
									if(isPrintable)
										if('\\' == c)
											fputs_unlocked("\\\\", stdout);
										else if('\'' == c)
											fputs_unlocked("\\'", stdout);
										else
											putc_unlocked(c, stdout);
									else if((unsigned)c < ansiEscapesLimit && ansiEscapes[c])
										printf("\\%c", ansiEscapes[c]);
									else
										printf(077 < c || ({
												//  Peak at the next character.  If it's not a
												// valid octal digit, print less than 3 digits.
												int32_t nextc;
												ungetc((nextc = getc_unlocked(stdin)), stdin);
												'7' >= nextc && '0' <= nextc;
											}) ? "\\%.3o" : "\\%o", c
										);
								while(0 < (c = getc_unlocked(stdin)) && ({
									isPrintable = isprint(c);
									1;
								}));
							}
							putc_unlocked('\'', stdout);
							'\'' == c;
						}))
							fputs_unlocked("\\\'", stdout);
						else if(0 >= c)
							break;
					}
					else
						putc_unlocked(c, stdout);
					c = getc_unlocked(stdin);
				} while(0 < c);
			}
			else
				fputs_unlocked("''", stdout);
		} while(0 == c ? (EOF != (c = getc_unlocked(stdin)) ? ({
					unsigned rc = ignoreNullInput || (
						nullTerminatedOutput ? EOF != putc_unlocked(0, stdout) && (
							! flushArguments || EOF != fflush_unlocked(stdout)
						) : EOF != putc_unlocked(' ', stdout)
					);
					if('~' == c && rc) {
						isPrintable = 1;
						goto narrowCharStartEscape;
					}
					rc;
				}) : ({
					if(nullTerminatedOutput)
						putc_unlocked(0, stdout);
					0;
				})
			) : ({
				if(ignoreNullInput && nullTerminatedOutput)
					putc_unlocked(0, stdout);
				0;
			})
		);
	}
	else if(localeIsNotUtf8) {
		//  The locale does not use UTF-8 encoding.  Deference is given to the library
		// including, unfortunately, its error handling.  This code has not been tested
		// in MS Windows (would a POSIX shell even work with UTF-16???)
		wint_t c = getwc_unlocked(stdin);
		unsigned isPrintable;
		if(L'~' == c) {
			isPrintable = 1;
			goto wideCharStartEscape;
		}
		do {
			if(0 != c && WEOF != c) {
				do {
					isPrintable = disableCQuoting || iswprintFn(c);
					if((c < sizeof(shControlChars) && shControlChars[c]) || ! isPrintable) {
						wideCharStartEscape:
						if(L'\'' == c || ({
							if(disableCQuoting) {
								putwc_unlocked(L'\'', stdout);
								do
									putwc_unlocked(c, stdout);
								while(0 != (c = getwc_unlocked(stdin)) && WEOF != c && '\'' != c);
							}
							else {
								fputws_unlocked(L"$'", stdout);
								do {
									if(isPrintable)
										if(L'\\' == c)
											fputws_unlocked(L"\\\\", stdout);
										else if(L'\'' == c)
											fputws_unlocked(L"\\'", stdout);
										else
											putwc_unlocked(c, stdout);
									else if(128 > c)
										if(c < ansiEscapesLimit && ansiEscapes[c])
											wprintf(L"\\%c", ansiEscapes[c]);
										else
											wprintf(077 < c || ({
													//  Peak at the next code point.  If it's not a
													// valid octal digit, print less than 3 digits.
													wint_t nextc;
													ungetwc((nextc = getwc_unlocked(stdin)), stdin);
													//  It is technically true that this comparison is
													// inappropriate when ! defined(__STDC_ISO_10646__)...
													// but it most likely holds up even then.
													L'7' >= nextc && L'0' <= nextc;
												}) ? L"\\%.3o" : L"\\%o", c
											);
									else if(! useUnicodeEscapes) {
										unsigned cbuff[7];
									#if 1
										unsigned * cbuffPtr;
										cbuff[6] = 0;
										cbuff[5] = (c & 0xBF) | 0x80;
										if(0x800 > c)
											*(cbuffPtr = cbuff + 4) = 0xC0 | c >> 6;
										else {
											cbuff[4] = (c >> 6 & 0xBF) | 0x80;
											if(0x10000 > c)
												*(cbuffPtr = cbuff + 3) = 0xE0 | c >> 12;
											else {
												cbuff[3] = (c >> 12 & 0xBF) | 0x80;
												if(0x200000 > c)
													*(cbuffPtr = cbuff + 2) = 0xF0 | c >> 18;
												else {
													cbuff[2] = (c >> 18 & 0xBF) | 0x80;
													if(0x4000000 > c)
														*(cbuffPtr = cbuff + 1) = 0xF8 | c >> 24;
													else {
														cbuff[1] = (c >> 24 & 0xBF) | 0x80;
														*(cbuffPtr = cbuff) = 0xFC | (c >= 0x40000000);
													}
												}
											}
										}
									#else
										unsigned * cbuffPtr = cbuff;
										if(0x800 > c) {
											*cbuff = 0xC0 | c >> 6;
											cbuff[1] = (c & 0xBF) | 0x80;
											cbuff[2] = 0;
										}
										else if(0x10000 > c) {
											*cbuff = 0xE0 | c >> 12;
											cbuff[1] = (c >> 6 & 0xBF) | 0x80;
											cbuff[2] = (c & 0x3F) | 0x80;
											cbuff[3] = 0;
										}
										else if(0x200000 > c) {
											*cbuff = 0xF0 | c >> 18;
											cbuff[1] = (c >> 12 & 0xBF) | 0x80;
											cbuff[2] = (c >> 6 & 0xBF) | 0x80;
											cbuff[3] = (c & 0xBF) | 0x80;
											cbuff[4] = 0;
										}
										else if(0x4000000 > c) {
											*cbuff = 0xF8 | c >> 24;
											cbuff[1] = (c >> 18 & 0xBF) | 0x80;
											cbuff[2] = (c >> 12 & 0xBF) | 0x80;
											cbuff[3] = (c >> 6 & 0xBF) | 0x80;
											cbuff[4] = (c & 0xBF) | 0x80;
											cbuff[5] = 0;
										}
										else {
											*cbuff = 0xFC | (c >= 0x40000000);
											cbuff[1] = (c >> 24 & 0xBF) | 0x80;
											cbuff[2] = (c >> 18 & 0xBF) | 0x80;
											cbuff[3] = (c >> 12 & 0xBF) | 0x80;
											cbuff[4] = (c >> 6 & 0xBF) | 0x80;
											cbuff[5] = (c & 0xBF) | 0x80;
											cbuff[6] = 0;
										}
									#endif
										do
											wprintf(L"\\%.3o", *cbuffPtr);
										while(*(++cbuffPtr));
									}
									else if(sizeof(wchar_t) > 2)
										if(65535 >= c)
											wprintf(0xFFF < c || ({
													wint_t nextc;
													ungetwc((nextc = getwc_unlocked(stdin)), stdin);
													iswxdigit(nextc);
												}) ? L"\\u%.4X" : L"\\u%X", c
											);
										else {
											//  In hopes of it taking less output glyphs,
											// only output as many UTF digits as are needed.
											// Terminate the quoting if it is followed by a
											// character that would be interpreted as a hex digit.
											wprintf(L"\\U%X", c);
											wint_t nextc;
											ungetwc((nextc = getwc_unlocked(stdin)), stdin);
											if(iswxdigit(nextc))
												break;
										}
									else
										wprintf(0xFFF < c || ({
												wint_t nextc;
												ungetwc((nextc = getwc_unlocked(stdin)), stdin);
												iswxdigit(nextc);
											}) ? L"\\u%.4X" : L"\\u%X", c
										);
								}
								while(0 != (c = getwc_unlocked(stdin)) && WEOF != c && ({
									isPrintable = iswprintFn(c);
									1;
								}));
							}
							putwc_unlocked(L'\'', stdout);
							L'\'' == c;
						}))
							fputws_unlocked(L"\\\'", stdout);
						else if(0 == c || WEOF == c)
							break;
					}
					else
						putwc_unlocked(c, stdout);
					c = getwc_unlocked(stdin);
				} while(0 != c && WEOF != c);
			}
			else
				fputws_unlocked(L"''", stdout);
		} while(0 == c ? (WEOF != (c = getwc_unlocked(stdin)) ? ({
					unsigned lc = ignoreNullInput || (
						nullTerminatedOutput ? WEOF != putwc_unlocked(L'\000', stdout) && (
							! flushArguments || EOF != fflush_unlocked(stdout)
						) : (WEOF != c && WEOF != putwc_unlocked(L' ', stdout))
					);
					if(L'~' == c && lc) {
						isPrintable = 1;
						goto wideCharStartEscape;
					}
					lc;
				}) : ({
					if(nullTerminatedOutput)
						putwc_unlocked(0, stdout);
					0;
				})
			) : ({
				if(ignoreNullInput && nullTerminatedOutput)
					putwc_unlocked(0, stdout);
				0;
			})
		);
		if(EILSEQ == errno)
			return EILSEQ;
	}
	else {
		//  The locale uses UTF-8 encoding
		//  It is apparently impossible to recover after a decoding error with getwc()
		// without closing the stream and losing input.  clearerr() and fflush() do
		// nothing to recover from a decoding error.  The next alternative is using a
		// regular byte oriented stream and the library's wide string conversion
		// functions, but recovering from errors with the conversion function requires
		// digging into the opaque mbstate_t object.  Hence getUtf8CodePoint(), so that
		// unrecognized characters can be pushed to the output without loss.
		//  When there is no error, getUtf8CodePoint() returns the next code point in the
		// stream.  cbuff and bytesInCodePoint are additional return values.
		//  Starting at index 0 of cbuff, bytesInCodePoint indicates how many bytes are
		// used by the returned code point.  This is set to zero if an encoding error or
		// EOF are detected, in which cases the values stored in cbuff should not be used.
		// When byteInCodePoint is 0, the return code will be the return code from getc()
		// at the location of the error.
		unsigned bytesInCodePoint;
		//  This buffer is intended for use by the caller for outputting raw UTF-8 w/o
		// coverting the code point back to UTF-8.
		unsigned char cbuff[4];
		//  getUtf8Buff and getUtf8BuffLength are used for internal tracking by
		// getUtf8CodePoint() between calls.
		int32_t getUtf8Buff[4];
		//  getUtf8BuffLength indicates how many addtional bytes have been read past the
		// returned code point or byte
		unsigned getUtf8BuffLength = 0;
		int32_t getUtf8CodePoint(void)
		{
			int32_t rc;
			if(! getUtf8BuffLength)
				*getUtf8Buff = getc_unlocked(stdin);
			if(-1 < *getUtf8Buff) {
				if(*getUtf8Buff & 0x80) {
					//  If getUtf8Buff[0]'s hsb-1 is not also set, it's invalid encoding.
					if(*getUtf8Buff & 0x40) {
						//  At least two characters are expected
						//int expectedBytes = c1 & 0x20 ? (c1 & 0x10 ? 4 : 3) : 2;
						if(2 > getUtf8BuffLength)
							getUtf8Buff[1] = getc_unlocked(stdin);
						if(*getUtf8Buff & 0x20) {
							//  At least 3 characters are expected
							if(3 > getUtf8BuffLength)
								getUtf8Buff[2] = getc_unlocked(stdin);
							if(*getUtf8Buff & 0x10) {
								if(! (*getUtf8Buff & 0x8)) {
									//  Exactly 4 characters are expected
									if(4 > getUtf8BuffLength)
										getUtf8Buff[3] = getc_unlocked(stdin);
									//  This condition will be false if getUtf8Buff[3] == EOF
									if((getUtf8Buff[3] & 0xC0) == 0x80) {
										rc = (int32_t)(*getUtf8Buff & 0x07) << 18
										| (int32_t)(getUtf8Buff[1] & 0x3F) << 12
										| (int32_t)(getUtf8Buff[2] & 0x3F) << 6
										| (int32_t)(getUtf8Buff[3] & 0x3F);
										//int32_t rcAndFFFF;
										if(0xFFFF < rc
											//  Code points > 0x10FFFF are invalid.
											&& 0x110000 > rc
											// Noncharacters are permissible.
											//&& 0xFFFF != (rcAndFFFF = rc & 0xFFFF)
											//&& 0xFFFE != rcAndFFFF
										) {
											bytesInCodePoint = 4;
											getUtf8BuffLength = 0;
											goto setCbuff3;
										}
									}
									getUtf8BuffLength = 4;
								}
							}
							else {
								//  Exactly 3 characters are expected
								//  This condition will be false if getUtf8Buff[2] == EOF
								if((getUtf8Buff[2] & 0xC0) == 0x80)
								{
									rc = (int32_t)(*getUtf8Buff & 0x0F) << 12
									| (int32_t)(getUtf8Buff[1] & 0x3F) << 6
									| (int32_t)(getUtf8Buff[2] & 0x3F);
									// 0x10000 > rc is guaranteed by the bit handling
									if(0x7FF < rc
										// Disallow UTF-16 surrogates
										&& (0xD800 > rc || 0xDFFF < rc)
										// Noncharacters are permissible.
										//&& (0xFDD0 > rc || 0xFDEF < rc) && 0xFFFE > rc
									) {
										bytesInCodePoint = 3;
										getUtf8BuffLength = getUtf8BuffLength > 3;
										goto setCbuff2;
									}
								}
							}
							if(3 > getUtf8BuffLength)
								getUtf8BuffLength = 3;
						}
						else {
							//  Exactly two characters are expected
							//  This condition will be false if getUtf8Buff[1] == EOF
							if((getUtf8Buff[1] & 0xC0) == 0x80 &&
								0x7F < (rc = (int32_t)(*getUtf8Buff & 0x1F) << 6 | (int32_t)(getUtf8Buff[1] & 0x3F))
								// 0x800 > rc is guaranteed by the bit handling
							) {
								bytesInCodePoint = 2;
								if(1 < getUtf8BuffLength)
									getUtf8BuffLength -= 2;
								else
									getUtf8BuffLength = 0;
								goto setCbuff1;
							}
							if(2 > getUtf8BuffLength)
								getUtf8BuffLength = 2;
						}
					}
					bytesInCodePoint = 0;
				}
				else {
					bytesInCodePoint = 1;
					*cbuff = *getUtf8Buff;
				}
			}
			else
				bytesInCodePoint = 0;
			rc = *getUtf8Buff;
			if(getUtf8BuffLength && --getUtf8BuffLength)
				memmove(getUtf8Buff, getUtf8Buff + 1, sizeof(int32_t) * getUtf8BuffLength);
			return rc;
		setCbuff3:
			cbuff[3] = getUtf8Buff[3];
		setCbuff2:
			cbuff[2] = getUtf8Buff[2];
		setCbuff1:
			cbuff[1] = getUtf8Buff[1];
			*cbuff = *getUtf8Buff;
			//  For the size of this buffer, there's little value in tracking the start index.
			// Shift it to begin at the zero index.
			if(getUtf8BuffLength)
				memmove(getUtf8Buff, getUtf8Buff + bytesInCodePoint, sizeof(int32_t) * getUtf8BuffLength);
			return rc;
		}
		void ungetUtf8CodePoint(int32_t uc)
		{
			if(bytesInCodePoint) {
				if(getUtf8BuffLength)
					memmove(getUtf8Buff + bytesInCodePoint, getUtf8Buff, sizeof(int32_t) * getUtf8BuffLength);
				getUtf8BuffLength += bytesInCodePoint;
				do {
					--bytesInCodePoint;
					getUtf8Buff[bytesInCodePoint] = (unsigned char )cbuff[bytesInCodePoint];
				}
				while(bytesInCodePoint);
			}
			else {
				//  This can lose valid input if called repeatedly.  Permitting a fifth character
				// in getUtf8Buff, which could only be populated as the result of an unget, would
				// avoid this.
				if(! getUtf8BuffLength)
					getUtf8BuffLength = 1;
				else if(4 > getUtf8BuffLength)
					memmove(getUtf8Buff + 1, getUtf8Buff, sizeof(int32_t) * (getUtf8BuffLength++));
				*getUtf8Buff = uc;
			}
		}
		int32_t c = getUtf8CodePoint();
		unsigned isPrintable;
		if('~' == c) {
			isPrintable = 1;
			goto utf8StartEscape;
		}
		do {
			if(0 < c) {
				do {
					//  Except when c <= 0, which has been ruled out, c will always be > 127
					// when bytesInCodePoint is 0
					if(! (isPrintable = bytesInCodePoint && iswprintFn((wint_t)c))
						|| ((uint32_t)c < sizeof(shControlChars) && shControlChars[c])
					) {
						if('\'' == c)
							fputs_unlocked("\\\'", stdout);
						else {
							utf8StartEscape:
							fputs_unlocked("$'", stdout);
							do {
								if(isPrintable)
									if('\\' == c)
										fputs_unlocked("\\\\", stdout);
									else if('\'' == c)
										fputs_unlocked("\\'", stdout);
									else if(bytesInCodePoint)
										fwrite_unlocked(cbuff, bytesInCodePoint, 1, stdout);
									else
										putc_unlocked(c, stdout);
								//  c will be in the range of 128 - 255 when ! bytesInCodePoint,
								// but there are valid code points in that range too
								else if(128 > c || ! bytesInCodePoint)
									if((uint32_t)c < ansiEscapesLimit && ansiEscapes[c])
										printf("\\%c", ansiEscapes[c]);
									else
										printf(077 < c || ({
												//  Peak at the next code point.  If it's not a
												// valid octal digit, print less than 3 digits.
												int32_t nextc;
												ungetUtf8CodePoint((nextc = getUtf8CodePoint()));
												'7' >= nextc && '0' <= nextc;
											}) ? "\\%.3o" : "\\%o", c
										);
								else if(! useUnicodeEscapes)
									//  Optimization is not possible in this case
									// since all bytes are > 127
									for(unsigned idx = 0; idx < bytesInCodePoint; idx++)
										printf("\\%.3o", (unsigned char)cbuff[idx]);
								else if(65535 >= c)
									printf(0xFFF < c || ({
											int32_t nextc;
											ungetUtf8CodePoint((nextc = getUtf8CodePoint()));
											nextc <= 'f' && isxdigit((wint_t)nextc);
										}) ? "\\u%.4X" : "\\u%X", c
									);
								else {
									//  In hopes of it taking less output glyphs,
									// only output as many UTF digits as are needed.
									// Terminate the quoting if it is followed by a
									// character that would be interpreted as a hex digit.
									printf("\\U%X", c);
									int32_t nextc;
									ungetUtf8CodePoint((nextc = getUtf8CodePoint()));
									if(nextc <= 'f' && isxdigit((wint_t)nextc))
										break;
								}
							}
							while(0 < (c = getUtf8CodePoint()) && ({
								isPrintable = bytesInCodePoint && iswprintFn((wint_t)c);
								1;
							}));
							putc_unlocked('\'', stdout);
							if(0 >= c)
								break;
						}
					}
					else
						fwrite_unlocked(cbuff, bytesInCodePoint, 1, stdout);
					c = getUtf8CodePoint();
				}
				while(0 < c);
			}
			else
				fputs_unlocked("''", stdout);
		} while(0 == c ? (EOF != (c = getUtf8CodePoint()) ? ({
					unsigned lc = ignoreNullInput || (
						nullTerminatedOutput ? EOF != putc_unlocked(0, stdout) && (
							! flushArguments || EOF != fflush_unlocked(stdout)
						) : EOF != putc_unlocked(' ', stdout)
					);
					if('~' == c && lc) {
						isPrintable = 1;
						goto utf8StartEscape;
					}
					lc;
				}) : ({
					if(nullTerminatedOutput)
						putc_unlocked(0, stdout);
					0;
				})
			) : ({
				if(ignoreNullInput && nullTerminatedOutput)
					putc_unlocked(0, stdout);
				0;
			})
		);
	}
	return 0;
}
