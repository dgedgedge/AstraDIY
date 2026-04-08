cmake_minimum_required(VERSION 3.16)
project(indi-astralim CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")

include(GNUInstallDirs)

set(INDI_ASTRALIM_VERSION_MAJOR 3)
set(INDI_ASTRALIM_VERSION_MINOR 0)

find_package(INDI REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Check for libgpiod - support both v1 and v2
pkg_check_modules(GPIOD REQUIRED libgpiod)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/indi_astralim.xml.cmake ${CMAKE_CURRENT_BINARY_DIR}/indi_astralim.xml)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${INDI_INCLUDE_DIR})
include_directories(${GPIOD_INCLUDE_DIRS})

link_directories(${GPIOD_LIBRARY_DIRS})

# Compiler flags
add_compile_options(-Wall -Wextra -Wpedantic)

# Shared GPIO library
set(gpio_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/astralim_gpio.cpp)

################ AstrAlim Focuser ################
add_executable(indi_astralim_focuser
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_focuser.cpp
    ${gpio_SRCS}
)
target_link_libraries(indi_astralim_focuser ${INDI_LIBRARIES} ${GPIOD_LIBRARIES})
install(TARGETS indi_astralim_focuser RUNTIME DESTINATION bin)

################ AstrAlim Relays ################
add_executable(indi_astralim_relays
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_relays.cpp
    ${gpio_SRCS}
)
target_link_libraries(indi_astralim_relays ${INDI_LIBRARIES} ${GPIOD_LIBRARIES})
install(TARGETS indi_astralim_relays RUNTIME DESTINATION bin)

################ AstrAlim System ################
add_executable(indi_astralim_system
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_system.cpp
)
target_link_libraries(indi_astralim_system ${INDI_LIBRARIES})
install(TARGETS indi_astralim_system RUNTIME DESTINATION bin)

################ AstrAlim Heater ################
add_executable(indi_astralim_heater
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_heater.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_ina219.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_com_actor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_com_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_com_fetcher.cpp
    ${gpio_SRCS}
)
target_link_libraries(indi_astralim_heater ${INDI_LIBRARIES} ${GPIOD_LIBRARIES} Threads::Threads)
install(TARGETS indi_astralim_heater RUNTIME DESTINATION bin)

################ AstraCom Runtime Test ################
add_executable(test_astra_com
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_astra_com.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_com_actor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_com_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/astralim_com_fetcher.cpp
)
target_link_libraries(test_astra_com Threads::Threads)

################ XML Driver Description ################
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/indi_astralim.xml DESTINATION ${INDI_DATA_DIR})
