include(GNUInstallDirs)

install(FILES sandman.conf sandman.sched invalidJson.sched DESTINATION 
        ${CMAKE_INSTALL_SYSCONFDIR}/sandman)
install(FILES sandman.log DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/sandman)
install(FILES reports/sandman.rpt DESTINATION ${CMAKE_INSTALL_LOCALSTATEDIR}/sandman/reports)
