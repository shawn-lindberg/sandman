bin_PROGRAMS = sandman
sandman_SOURCES = config.cpp command.cpp control.cpp input.cpp logger.cpp mqtt.cpp notification.cpp reports.cpp schedule.cpp sound.cpp timer.cpp xml.cpp main.cpp 
sandman_CPPFLAGS = $(XML_CFLAGS) $(SDL_MIXER_CFLAGS) -DAM_DATADIR='"$(datadir)/sandman/"' -DAM_CONFIGDIR='"$(sysconfdir)/sandman/"' -DAM_TEMPDIR='"$(localstatedir)/sandman/"'
sandman_LDADD = $(XML_LIBS) $(SDL_MIXER_LIBS)
