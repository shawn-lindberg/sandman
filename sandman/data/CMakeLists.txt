include(GNUInstallDirs)

# We need to copy files required for tests the build directory.
file(COPY sandman.conf sandman.rtn invalid_json.rtn example.rtn DESTINATION 
	  ${CMAKE_CURRENT_BINARY_DIR})
