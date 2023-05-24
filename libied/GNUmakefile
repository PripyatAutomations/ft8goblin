VERSION := 20230522
all: world

include ../mk/config.mk
CFLAGS += -I./ -I../ -I../ext/
LDFLAGS := ${SAN_FLAGS} -L../lib/
my_libs := ied
libied := libied.so
include rules.mk
build_libs := $(foreach x,${my_libs},../lib/lib${x}.so)
libied_real_objs := $(foreach x,${libied_objs},obj/${x})
extra_clean += ../lib/${libied} ${libied_real_objs}

dep_libs := ev yajl
real_dep_libs := $(foreach x,${dep_libs},-l${x})

#src/%.c: GNUmakefile $(wildcard *.h)
../lib/${libied}: ${libied_real_objs} 
	@echo "[SO] ${libied_real_objs} -> $@"
	${CC} ${LDFLAGS} -fPIC -shared -Wl,-soname,${libied} -lm -o $@ ${libied_real_objs} ${real_dep_libs}

install_extra_targets += install-libied

include ../mk/compile.mk
include ../mk/clean.mk
include ../mk/install.mk
include ../mk/help.mk

world: ${build_libs}

install-libied:
	install -m0644 ../lib/${libied} ${lib_install_path}
