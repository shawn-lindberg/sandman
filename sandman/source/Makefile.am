bin_PROGRAMS = sandman
sandman_SOURCES = config.cpp command.cpp control.cpp input.cpp logger.cpp mqtt.cpp ncurses_context.cpp notification.cpp reports.cpp schedule.cpp timer.cpp main.cpp 
sandman_CPPFLAGS = -DSANDMAN_CONFIG_DIR='"$(sysconfdir)/sandman/"' -DSANDMAN_TEMP_DIR='"$(localstatedir)/sandman/"'
if ENABLE_GPIO
sandman_CPPFLAGS += -DENABLE_GPIO
endif
