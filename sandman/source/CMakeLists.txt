include(GNUInstallDirs)

set(SANDMAN_LIB_SOURCE_FILES command.cpp config.cpp control.cpp gpio.cpp input.cpp logger.cpp
	mqtt.cpp notification.cpp reports.cpp routines.cpp shell.cpp timer.cpp)
add_library(sandman_lib STATIC ${SANDMAN_LIB_SOURCE_FILES})

add_executable(sandman main.cpp)

add_library(sandman_compiler_flags INTERFACE)
target_compile_features(sandman_compiler_flags INTERFACE cxx_std_17)

option(ENABLE_GPIO "Whether to use Raspberry Pi GPIO or not." ON)
message(STATUS "ENABLE_GPIO = ${ENABLE_GPIO}")
if (ENABLE_GPIO)
	target_compile_definitions(sandman_lib PUBLIC ENABLE_GPIO)
endif()

find_package(Curses REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Mosquitto IMPORTED_TARGET libmosquitto REQUIRED)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(sandman_lib PUBLIC
		# Use sanitizers in debug build.
		$<$<CONFIG:Debug>:-fsanitize=address,undefined>

		-Wall                      # Enable most common warnings.
		-Wextra                    # Enable additional warnings.
		$<$<CONFIG:Debug>:-Werror> # In debug build, treat warnings as errors.
		-Wpedantic                 # Enforce strict ISO C++ compliance.
		-Wshadow                   # Warn when a variable shadows another.
		-Wnon-virtual-dtor         # Warn if class with virtual functions has non-virtual destructor.
		-Wold-style-cast           # Warn about old-style casts (from C).
		-Woverloaded-virtual       # Warn if a virtual function is overloaded.
		# -Wsign-conversion        # Warn on implicit conversions that change sign.
		-Wdiv-by-zero              # Warn on divide by zero.
		-Wformat=2                 # Warn about format string issues.
		-Wfloat-equal              # Warn if floating-point values are compared for equality.
		-Wimplicit-fallthrough     # Warn if a switch case fallthrough is missing `[[fallthrough]]`.
		-Wdeprecated-declarations  # Warn on usage of constructs marked with `[[deprecated]]`.

		# Don't treat usage of constructs marked with `[[deprecated]]` as errors.
		-Wno-error=deprecated-declarations
	)

	target_link_options(sandman_lib PUBLIC
		$<$<CONFIG:Debug>:-fsanitize=address,undefined>
	)

else()
	message(WARNING "Not using GNU compiler or Clang compiler.")
endif()

target_include_directories(sandman_lib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(sandman_lib PUBLIC sandman_compiler_flags ${CURSES_LIBRARIES}
							 PkgConfig::Mosquitto)

if (ENABLE_GPIO)
	target_link_libraries(sandman_lib PUBLIC gpiod)
endif()

target_link_libraries(sandman PUBLIC sandman_compiler_flags sandman_lib ${CURSES_LIBRARIES})

install(TARGETS sandman DESTINATION bin)
