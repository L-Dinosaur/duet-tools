GZIPCMD=@gzip
INSTALL= install

prefix ?= /usr/local
bindir = $(prefix)/bin
mandir = $(prefix)/man
man8dir = $(mandir)/man8

# clear out all suffixes
.SUFFIXES:
# list only those we use
.SUFFIXES: .in .gz

MANPAGES = duet.8.gz
INFILES = ${MANPAGES:.in=.gz}

all: $(MANPAGES)

.in.gz :
	@echo "    [MAN]    $@"
	$(GZIPCMD) -n -c $< > $@

clean :
	rm -f $(MANPAGES)

install: $(MANPAGES)
	$(INSTALL) -m755 -d $(DESTDIR)$(man8dir)
	$(INSTALL) -m 644 $(MANPAGES) $(DESTDIR)$(man8dir)
