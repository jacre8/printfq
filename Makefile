prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin

BINDIR = bin
VPATH = $(BINDIR)

CC ?= gcc
CFLAGS ?= -s -O2
CFLAGS += -Wall -Wshadow -Wimplicit -Wextra -Winline -Wundef -Wmissing-declarations \
-Wstrict-prototypes -Wmissing-prototypes -Wno-unused-parameter -Wtrampolines

printfq : printfq.c | $(BINDIR)
	$(CC) $(CFLAGS) $(PRITNFQ_FLAGS) -o $(BINDIR)/printfq $^

.PHONY : install
install : $(DESTDIR)$(bindir)/printfq

.PHONY : clean
clean :
	rm -f $(BINDIR)/printfq

$(DESTDIR)$(bindir)/printfq : printfq | $(DESTDIR)$(bindir)
	install -o root -g root -m 0755 bin/printfq "$(DESTDIR)$(bindir)"

$(DESTDIR)$(bindir) : | $(DESTDIR)$(exec_prefix)

$(DESTDIR)$(exec_prefix) : | $(DESTDIR)

#  Directory targets.
$(BINDIR) $(DESTDIR) $(DESTDIR)$(exec_prefix) $(DESTDIR)$(bindir) :
	@if ! [ -d "$@" ] && ! mkdir -p "$@"; then \
		echo Error. Unable to create the output directory: "$@"; \
		exit 1; \
	fi
