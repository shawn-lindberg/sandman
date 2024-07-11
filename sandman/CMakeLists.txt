cmake_minimum_required(VERSION 3.10)
project(Sandman)

add_subdirectory(source)
add_subdirectory(data)

find_package(Doxygen)
if(DOXYGEN_FOUND)
	message(STATUS "Doxygen found: `${DOXYGEN_EXECUTABLE}` version ${DOXYGEN_VERSION}")

	set(DOXYGEN_PROJECT_NAME Sandman)
	set(DOXYGEN_PROJECT_BRIEF "A device to allow people to control hospital style beds by voice.")
	
	set(DOXYGEN_EXCLUDE_PATTERNS ${PROJECT_SOURCE_DIR}/source/rapidjson/**)

	set(DOXYGEN_GENERATE_HTML YES)
	set(DOXYGEN_GENERATE_MAN NO)
	set(DOXYGEN_GENERATE_LATEX NO)
	set(DOXYGEN_GENERATE_XML NO)

	doxygen_add_docs(
		documentation
		${PROJECT_SOURCE_DIR}/source
		comment "Generating documentation with Doxygen."
	)

else()
	message(WARNING "Doxygen not found. Documentation will not be generated.")
endif()

