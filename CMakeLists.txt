cmake_minimum_required(VERSION 3.28...3.30)
project(screenRecorder LANGUAGES C CXX)

find_package(PkgConfig REQUIRED)
find_package(Stb REQUIRED)

pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3>=0.3.33)
pkg_check_modules(GLIB REQUIRED gio-2.0>=2.76)

include_directories(
        ${PIPEWIRE_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
        ${LIBDRM_INCLUDE_DIRS}
)

set(SRC_FILES
        src/main.cpp
        src/screencast-portal.cpp
        src/portal.cpp
        src/pipewire.cpp
)

add_executable(screenRecorder ${SRC_FILES})

target_include_directories(screenRecorder PRIVATE ${Stb_INCLUDE_DIR})
target_link_libraries(screenRecorder
        ${PIPEWIRE_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${LIBDRM_LIBRARIES}
)
