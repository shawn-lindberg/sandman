bin_PROGRAMS = sandman
sandman_SOURCES = config.cpp control.cpp logger.cpp speech_recognizer.cpp timer.cpp main.cpp
sandman_CPPFLAGS = -I$(srcdir)/../../sphinxbase/include -I$(srcdir)/../../pocketsphinx/include -DAM_DATADIR='"$(datadir)/sandman/"' \
	-DAM_CONFIGDIR='"$(sysconfdir)/sandman/"'
sandman_LDADD = $(srcdir)/../../sphinxbase/src/libsphinxad/libsphinxad.la $(srcdir)/../../pocketsphinx/src/libpocketsphinx/libpocketsphinx.la
