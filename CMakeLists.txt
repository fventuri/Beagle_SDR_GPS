cmake_minimum_required(VERSION 3.13)
project(RaspSDR VERSION 1.0.428)

find_package(PkgConfig)
INCLUDE(GNUInstallDirs)

pkg_check_modules(FFTW3 REQUIRED fftw3f)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    MESSAGE("CCache is Enabled")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif(CCACHE_FOUND)

file(GLOB ASM_SRCS "e_cpu/asm/*.cpp")
add_executable(asm ${ASM_SRCS})
target_include_directories(asm PUBLIC "e_cpu/asm" "e_cpu")

add_custom_target(kiwi_gen_h
    COMMAND asm -o ${PROJECT_BINARY_DIR}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/e_cpu/"
    DEPENDS asm
    SOURCES ${ASM_SRCS}
)

SET(EXT_SRCS ${PROJECT_BINARY_DIR}/extint.cpp)

SET(EXTENSIONS 
    colormap
    cw_decoder
    wspr
    fax
    fsk
    FFT
    IBP_scan
    iframe
    iq_display
    loran_c
    navtex
    noise_blank
    noise_filter
    S_meter
    s4285
    sig_gen
    SSTV
    timecode
)

SET(GEN_EXT_MAIN "// auto-generated file -- do not edit by hand\n" )
STRING(APPEND GEN_EXT_MAIN "void extint_init() {\n")

FOREACH(ext_name ${EXTENSIONS})
    FILE(GLOB SRCS
        "extensions/${ext_name}/*.cpp"
        "extensions/${ext_name}/*/*.cpp"
    )
    LIST(APPEND EXT_SRCS ${SRCS})

    STRING(APPEND GEN_EXT_MAIN "\textern void ${ext_name}_main()\;\n${ext_name}_main()\;\n")

ENDFOREACH()

STRING(APPEND GEN_EXT_MAIN "}")
FILE(GENERATE
    OUTPUT ${PROJECT_BINARY_DIR}/extint.cpp 
    CONTENT ${GEN_EXT_MAIN}
)

SET(CPU BCM2837)
SET(ARCH omap)
SET(PLATFORM raspberrypi)

file(GLOB KIWI_SRCS
    "main.cpp"
    "rx/*.cpp"
    "rx/*/*.cpp"
    "gps/*.cpp"
    "web/*.cpp"
    "net/*.cpp"
    "support/*.cpp"
    "ui/*.cpp"
    "gps/*.cpp"
    "gps/*/*.cpp"
    "init/*.cpp"
    "extensions/*.cpp"

    "platform/common/*.cpp"
    "platform/${PLATFORM}/*.cpp"

    "pkgs/TNT_JAMA/*.cpp"
    "pkgs/jsmn/*.cpp"
    "pkgs/mongoose/*.cpp"
    "pkgs/sha256/*.cpp"
    )

add_executable(kiwi ${KIWI_SRCS} ${EXT_SRCS})
add_dependencies(kiwi kiwi_gen_h)

target_include_directories(kiwi PUBLIC
    ${PROJECT_BINARY_DIR}
    "."
    "platform/common"

    "support"
    "init"
    "rx"
    "web"
    "gps"
    "rx/CuteSDR"
    "rx/csdr"
    "rx/Teensy"
    "rx/kiwi"
    "rx/wdsp"
    "rx/CMSIS"
    "net"

    "extensions"
    "extensions/noise_blank"
    "extensions/wspr"
    "extensions/DRM"
    "extensions/noise_filter"
    "extensions/DRM/dream"

    "pkgs/TNT_JAMA"
    "pkgs/jsmn"
    "pkgs/mongoose"
    "pkgs/sha256"

    "gps/ka9q-fec"
    "gps/GNSS-SDRLIB"
)

target_include_directories(kiwi PUBLIC "platform/${PLATFORM}")
target_include_directories(kiwi PUBLIC ${FFTW3_INCLUDE_DIRS})

target_link_directories(kiwi PUBLIC
        ${GLFW3_LIBRARY_DIRS}
        )

target_link_libraries(kiwi PUBLIC
    ${FFTW3_STATIC_LIBRARIES}
    util
    )

install(TARGETS kiwi
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY web/openwebrx
    DESTINATION "${CMAKE_INSTALL_DATADIR}/RaspSDR/kiwi"
)

install(DIRECTORY web/extensions
    DESTINATION "${CMAKE_INSTALL_DATADIR}/RaspSDR/kiwi"
    PATTERN "Makefile" EXCLUDE
)

install(DIRECTORY web/kiwi
    DESTINATION "${CMAKE_INSTALL_DATADIR}/RaspSDR"
    PATTERN "Makefile" EXCLUDE
)

install(DIRECTORY config
    DESTINATION "${CMAKE_INSTALL_DATADIR}/RaspSDR/config"
    PATTERN "Makefile" EXCLUDE
)

file(GLOB FIRMWARES *SDR.rx*.bit)
install(FILES ${FIRMWARES} 
    DESTINATION "${CMAKE_INSTALL_DATADIR}/RaspSDR/firmware"
)

install(FILES unix_env/kiwid
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/init.d"
)

install(FILES unix_env/kiwid.service
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/systemd/system"
)

cmake_host_system_information(RESULT _host_name QUERY HOSTNAME)

target_compile_definitions(kiwi PUBLIC 
    VERSION_MAJ=1 VERSION_MIN=248
    KIWI KIWISDR MULTI_CORE DRM
    ARCH_${ARCH} CPU_${CPU} ARCH_CPU=${CPU}
    DIR_CFG="config"
    CFG_PREFIX="dist."
    REPO_NAME="Beagle_SDR_GPS"
    BUILD_DIR="${PROJECT_BINARY_DIR}"
    COMPILE_HOST="${_host_name}"
    )

SET(CPACK_PACKAGE_NAME "RaspSDR")
SET(CPACK_PACKAGE_VERSION_MAJOR 1)
SET(CPACK_PACKAGE_VERSION_MINOR 428)
SET(CPACK_PACKAGE_VERSION_PATCH 0)
SET(CPACK_GENERATOR "DEB")
SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "Howard Su")
SET(CPACK_DEBIAN_PACKAGE_DESCRIPTION "RaspberryPi based WebSDR solution, which is derived from KiwiSDR")
SET(CPACK_DEBIAN_PACKAGE_DEPENDS "libfftw3-single3 curl wget avahi-daemon avahi-utils libnss-mdns avahi-autoipd miniupnpc dnsutils pnmtopng ethtool psmisc jq zlib1g libsndfile1")
INCLUDE(CPack)