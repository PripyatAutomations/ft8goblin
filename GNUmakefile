VERSION = 20230522
all: world
bins := ft8goblin decoderd-ft8 encoderd-ft8 sigcapd flac-streamerd

include mk/config.mk
include mk/ft8lib.mk
include mk/termbox2.mk

extra_distclean += etc/calldata-cache.db etc/fcc-uls.db

ft8goblin_objs += ft8goblin.o	# main TUI program
ft8goblin_objs += hamlib.o	# hamlib (rigctld) interface
ft8goblin_objs += tui-input.o
ft8goblin_objs += watch.o	# watch lists
ft8coder_objs += ft8lib.o	# interface to the FT8 library
ft8decoder_objs += decoderd-ft8.o ${ft8coder_objs}
ft8encoder_objs += encoderd-ft8.o ${ft8coder_objs}
sigcapd_objs += sigcapd.o
sigcapd_objs += uhd.o
sigcapd_objs += hamlib.o
sigcapd_objs += alsa.o		# ALSA Linux Audio
flac_streamerd_objs += flac-streamerd.o

ifeq (${PULSEAUDIO}, y)
sigcapd_objs += pulse.o		# pulseaudio
sigcapd_cflags += -DPULSEAUDIO
# make these force rebuild of obj/pulse.o
obj/sigcapd.o: include/config.h mk/config.mk
else
extra_clean += obj/pulse.o
endif
sigcapd_objs += udp_src.o	# UDP input (gnuradio, gqrx, etc)

# Explode file names as needed for building
ft8goblin_real_objs := $(foreach x,${ft8goblin_objs},obj/${x})
ft8decoder_real_objs := $(foreach x,${ft8decoder_objs},obj/${x})
ft8encoder_real_objs := $(foreach x,${ft8encoder_objs},obj/${x})
sigcapd_real_objs := $(foreach x,${sigcapd_objs},obj/${x})
flac_streamerd_real_objs := $(foreach x,${flac_streamerd_objs},obj/${x})

extra_build_targets += etc/calldata-cache.db
real_bins := $(foreach x,${bins},bin/${x})
extra_clean += ${ft8goblin_real_objs} ${ft8decoder_real_objs} ${ft8encoder_real_objs} ${sigcapd_real_objs} ${flac_streamerd_real_objs}
extra_clean += ${real_bins} ${ft8lib} ${ft8lib_objs}

#################
# Build Targets #
#################
bin/ft8goblin: ${ft8goblin_real_objs} | lib/libtermbox2.so lib/libied.so
	@echo "[Linking] $@"
	${CC} -o $@ ${ft8goblin_real_objs} ${ft8goblin_ldflags} ${LDFLAGS}

bin/decoderd-ft8: ${ft8decoder_real_objs} ${ft8lib}
	@echo "[Linking] $@"
	${CC} -o $@ ${ft8decoder_real_objs} ${ft8coder_ldflags} ${LDFLAGS}

bin/encoderd-ft8: ${ft8encoder_real_objs} ${ft8lib}
	@echo "[Linking] $@"
	${CC} -o $@ ${ft8encoder_real_objs} ${ft8coder_ldflags} ${LDFLAGS}

bin/sigcapd: ${sigcapd_real_objs}
	@echo "[Linking] $@"
	@${CXX} -o $@ ${sigcapd_real_objs} ${sigcapd_ldflags} ${LDFLAGS}

bin/flac-streamerd: ${flac_streamerd_real_objs}
	@echo "[Linking] ${flac_streamerd_real_objs} to $@"
	${CXX} -o $@ ${flac_streamerd_real_objs} ${flac_streamerd_ldflags} ${LDFLAGS}

obj/sigcapd.o: src/sigcapd.cc
	@echo "[CXX] $^ -> $@"
	${CXX} ${CXXFLAGS} ${sigcapd_cflags} -o $@ -c $<

##########
etc/calldata-cache.db:
	sqlite3 etc/calldata-cache.db < sql/cache.sql 

lib/libied.so:
	${MAKE} -C libied world

extra_clean_targets += libied_clean
libied_clean:
	${MAKE} -C libied clean

# Build all subdirectories first, then our binary
world: ${extra_build_targets} ${real_bins} bin/callsign-lookup

todo:
	# We would use find here, but there's probably XXX: in subdirs we don't care about...
	grep -Hn "XXX:" include/* src/* * etc/* sql/* scripts/* mk/* 2>/dev/null | less

include mk/compile.mk
include mk/yajl.mk
include mk/help.mk
include mk/callsign-lookup.mk
include mk/clean.mk
include mk/install.mk
