bin_PROGRAMS = sandman
sandman_SOURCES = config.cpp command.cpp control.cpp input.cpp logger.cpp mqtt.cpp notification.cpp reports.cpp schedule.cpp sound.cpp timer.cpp xml.cpp main.cpp 
if SPHINX_SPEECH_RECOGNITION
sandman_SOURCES += speech_recognizer.cpp 
endif
sandman_CPPFLAGS = $(XML_CFLAGS) $(SDL_MIXER_CFLAGS) -DAM_DATADIR='"$(datadir)/sandman/"' -DAM_CONFIGDIR='"$(sysconfdir)/sandman/"' -DAM_TEMPDIR='"$(localstatedir)/sandman/"'
if SPHINX_SPEECH_RECOGNITION
sandman_CPPFLAGS += -DUSE_INTERNAL_SPEECH_RECOGNITION -I$(srcdir)/../../sphinxbase/include -I$(srcdir)/../../pocketsphinx/include
endif
sandman_LDADD = $(XML_LIBS) $(SDL_MIXER_LIBS)
if SPHINX_SPEECH_RECOGNITION
sandman_LDADD += $(srcdir)/../../sphinxbase/src/libsphinxad/libsphinxad.la \
	$(srcdir)/../../pocketsphinx/src/libpocketsphinx/libpocketsphinx.la
endif
