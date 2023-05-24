# Are we building debug? y or n.
DEBUG=n
PREFIX ?= /usr
PULSEAUDIO=y
ALSA=n
POSTGRESQL=n

termbox_lib_ver := 2.0.0

bin_install_path := ${PREFIX}/bin
etc_install_path ?= /etc/ft8goblin
lib_install_path := ${PREFIX}/lib/x86_64-linux-gnu/

# required libraries: -l${x} will be expanded later...
common_libs += yajl ev ied sqlite3 m termbox2
ft8goblin_libs += hamlib
ft8coder_libs +=
flac_streamerd_libs +=
sigcapd_libs += hamlib # uhd rtlsdr
callsign_lookup_libs := curl

# If building DEBUG release
ifeq (${DEBUG},y)
WARN_FLAGS := -Wall -pedantic -Wno-unused-variable -Wno-unused-function #-Wno-missing-braces
ERROR_FLAGS += -Werror 
# Sanitizer options
SAN_FLAGS := -fsanitize=address
SAN_LDFLAGS := ${SAN_FLAGS} -lasan
OPT_FLAGS += -ggdb3 -fno-omit-frame-pointer
endif
OPT_FLAGS += -O2
CFLAGS += ${SAN_FLAGS} ${WARN_FLAGS} ${ERROR_FLAGS} ${OPT_FLAGS} -DDEBUG=1

C_STD := -std=gnu11
CXX_STD := -std=gnu++17
CFLAGS += ${C_STD} -I./ext/ -I./include/ -I./ext/ft8_lib/ -I./ -I../ -fPIC
CFLAGS += -DVERSION="\"${VERSION}\""
CXXFLAGS := ${CXX_STD} $(filter-out ${C_STD},${CFLAGS})
LDFLAGS += -L./lib/
#LDFLAGS += $(foreach x,${common_libs},-l${x})
ft8lib_cflags := -fPIC


# Adjust for build-time options
ifeq (${ALSA},y)
libs += asound
CFLAGS += -DUSE_ALSA
endif

ifeq (${PULSEAUDIO},y)
libs += pulse
CFLAGS += -DUSE_PULSEAUDIO
endif

ifeq (${POSTGRESQL},y)
ft8goblin_libs += pq
callsign_lookup_libs += pq
CFLAGS += -DUSE_POSTGRESQL
endif

ft8goblin_ldflags := ${LDFLAGS} $(foreach x,${ft8goblin_libs} ${common_libs},-l${x})
ft8coder_ldflags := ${LDFLAGS} $(foreach x,${ft8coder_libs} ${common_libs},-l${x})
ft8coder_ldflags += -lft8
sigcapd_ldflags := ${LDFLAGS} $(foreach x,${sigcapd_libs} ${common_libs},-l${x})
flac_streamerd_ldflags := ${LDFLAGS} $(foreach x,${flac_streamerd_libs} ${common_libs},-l${x})
callsign_lookup_ldflags := ${LDFLAGS} $(foreach x,${callsign_lookup_libs} ${common_libs},-l${x})
