include(GNUInstallDirs)

set(SOURCE_FILES main.cpp command.cpp config.cpp control.cpp input.cpp logger.cpp mqtt.cpp 
    notification.cpp reports.cpp schedule.cpp timer.cpp xml.cpp)

add_executable(sandman ${SOURCE_FILES})

add_library(sandman_compiler_flags INTERFACE)
target_compile_features(sandman_compiler_flags INTERFACE cxx_std_17)

option(ENABLE_GPIO "Whether to use Raspberry Pi GPIO or not." ON)
message(STATUS "ENABLE_GPIO = ${ENABLE_GPIO}")
if (ENABLE_GPIO)
    target_compile_definitions(sandman PRIVATE ENABLE_GPIO)
endif()

target_compile_definitions(sandman 
                           PRIVATE SANDMAN_CONFIG_DIR="${CMAKE_INSTALL_FULL_SYSCONFDIR}/sandman/"
                           PRIVATE SANDMAN_TEMP_DIR="${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/sandman/")

#configure_file(sandman_config.h.in sandman_config.h)

find_package(Curses REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(Mosquitto IMPORTED_TARGET libmosquitto REQUIRED)

target_compile_options(sandman PUBLIC -Wall -Werror)
target_include_directories(sandman PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")

target_link_libraries(sandman PUBLIC sandman_compiler_flags ${CURSES_LIBRARIES} LibXml2::LibXml2 
                      PkgConfig::Mosquitto)
if (ENABLE_GPIO)
    target_link_libraries(sandman PUBLIC pigpio)
endif()

install(TARGETS sandman DESTINATION bin)