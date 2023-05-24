################################
# Support for building termbox #
################################
termbox2 := lib/libtermbox2.so.${termbox_lib_ver}

termbox2: ${termbox2}

lib/libtermbox2.so.${termbox_lib_ver}: ext/termbox2/libtermbox2.so.${termbox_lib_ver}
	@echo "[CP] $@ lib/"
	@install -m 0755 $^ lib/

lib/libtermbox2.so.2: lib/libtermbox2.so.${termbox_lib_ver}
	@echo "[LN] $< -> $@"
	@rm -f $@
	@ln -s $(shell basename $<) $@

lib/libtermbox2.so: lib/libtermbox2.so.2
	@echo "[LN] $< -> $@"
	@rm -f $@
	@ln -s $(shell basename $<) $@

ext/termbox2/libtermbox2.so.${termbox_lib_ver}: $(wildcard ext/termbox2/*.h ext/termbox2/*.c) ext/termbox2/Makefile
	@${MAKE} -C ext/termbox2 all

termbox2-clean:
	${MAKE} -C ext/termbox2 clean
	${RM} -f lib/libtermbox2.* ${termbox2}

extra_clean_targets += termbox2-clean
extra_build_targets += ${termbox2}
extra_install_targets += termbox2-install

termbox2-install:
	install -m 0755 ${termbox2} ${lib_install_path}
	${RM} -f ${lib_install_path}/libtermbox2.so.2 ${lib_install_path}/libtermbox2.so
	ln -s ${termbox2} ${lib_install_path}/libtermbox2.so.2
	ln -s ${termbox2} ${lib_install_path}/libtermbox2.so
