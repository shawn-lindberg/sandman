include(GNUInstallDirs)

# We need to copy files required for tests the build directory.
file(COPY sandman.conf sandman.sched invalidJson.sched example.sched DESTINATION 
	  ${CMAKE_CURRENT_BINARY_DIR})

install(FILES sandman.conf sandman.sched DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/sandman)
