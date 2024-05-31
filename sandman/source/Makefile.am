bin_PROGRAMS = sandman
sandman_SOURCES = config.cpp command.cpp control.cpp input.cpp logger.cpp mqtt.cpp notification.cpp reports.cpp schedule.cpp timer.cpp xml.cpp main.cpp 
sandman_CPPFLAGS = $(XML_CFLAGS) -DSANDMAN_CONFIG_DIR='"$(sysconfdir)/sandman/"' -DSANDMAN_TEMP_DIR='"$(localstatedir)/sandman/"'
if ENABLE_GPIO
sandman_CPPFLAGS += -DENABLE_GPIO
endif
sandman_LDADD = $(XML_LIBS)
