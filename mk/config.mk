PREFIX ?= /usr
PULSEAUDIO=y
ALSA=y
POSTGRESQL=y

termbox_lib_ver := 2.0.0

bin_install_path := ${PREFIX}/bin
etc_install_path ?= /etc/ft8goblin
lib_install_path := ${PREFIX}/lib

# required libraries: -l${x} will be expanded later...
common_libs += yajl ev
ft8goblin_libs += ncurses termbox2 hamlib m
ft8coder_libs += m
sigcapd_libs += uhd rtlsdr uhd rtlsdr hamlib
callsign_lookupd_libs := m curl

ERROR_FLAGS := -Werror 
#SAN_FLAGS := -fsanitize=address
WARN_FLAGS := -Wall -pedantic -Wno-unused-variable -Wno-unused-function #-Wno-missing-braces
OPT_FLAGS := -O2 -ggdb3
C_STD := -std=gnu11
CXX_STD := -std=gnu++17
CFLAGS += ${C_STD} -I./ext/ -I./include/ -I./ext/ft8_lib/ -fPIC
CFLAGS += ${SAN_FLAGS} ${WARN_FLAGS} ${ERROR_FLAGS} ${OPT_FLAGS}
CFLAGS += -DVERSION="\"${VERSION}\""
CXXFLAGS := ${CXX_STD} $(filter-out ${C_STD},${CFLAGS})
LDFLAGS += -L./lib/ ${SAN_FLAGS}
LDFLAGS += $(foreach x,${common_libs},-l${x})
ft8lib_cflags := -fPIC
ft8goblin_ldflags := ${LDFLAGS} $(foreach x,${ft8goblin_libs},-l${x})
ft8coder_ldflags := ${LDFLAGS} $(foreach x,${ft8coder_libs},-l${x})
ft8coder_ldflags += -lft8
sigcapd_ldflags := ${LDFLAGS} $(foreach x,${sigcapd_libs},-l${x})
callsign_lookupd_ldflags := ${LDFLAGS} $(foreach x,${callsign_lookupd_libs},-l${x})
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
CFLAGS += -DUSE_POSTGRESQL
endif
