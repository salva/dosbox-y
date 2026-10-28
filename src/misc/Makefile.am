AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/src "-DRESDIR=\"$(resdir)\""

resdir = $(datarootdir)/dosbox-x

noinst_LIBRARIES = libmisc.a
noinst_HEADERS = state_archive_output.h state_checkpoint_stream.h

# Re-evaluate source/configuration inputs on incremental and exported builds.
.PHONY: state-identity-force
state_build_identity.h: state-identity-force
	python3 $(top_srcdir)/scripts/state-build-identity.py $(top_srcdir) $(top_builddir) $@
savestates.o: state_build_identity.h
BUILT_SOURCES = state_build_identity.h
CLEANFILES = state_build_identity.h
libmisc_a_SOURCES = clipboard.cpp cross.cpp ethernet.cpp ethernet_pcap.cpp ethernet_slirp.cpp ethernet_ethnet.cpp ethernet_nothing.cpp localpipe.cpp messages.cpp programs.cpp setup.cpp support.cpp regionalloctracking.cpp savestates.cpp shiftjis.cpp iconvpp.cpp mkdir_p.cpp
