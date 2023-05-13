yajl_cflags += $(pkgconf --cflags yajl)
yajl_ldflags += $(pkgconf --ldflags yajl)

yajl_lib_ver := 2.1.1
yajl := lib/libyajl.so

lib/libyajl.so.${yajl_lib_ver}: ext/yajl/build/yajl-2.1.1/lib/libyajl.so.${yajl_lib_ver}
	@echo "[CP] $@ lib/"
	@install -m 0755 $^ lib/

lib/libyajl.so.2: lib/libyajl.so.${yajl_lib_ver}
	@echo "[LN] $< -> $@"
	@rm -f $@
	@ln -s $(shell basename $<) $@

lib/libyajl.so: lib/libyajl.so.2
	@echo "[LN] $< -> $@"
	@rm -f $@
	@ln -s $(shell basename $<) $@

ext/yajl/build/yajl-${yajl_lib_ver}/lib/libyajl.so.${yajl_lib_ver}: ext/yajl/Makefile
	@${MAKE} -C ext/yajl

ext/yajl/Makefile:
	@cd ext/yajl; ./configure --prefix=.

extra_clean += ${yajl} ${yajl}.so.2 ${yajk}.so.${yajl_lib_ver}
extra_clean_targets += yajl-clean
extra_build_targets += ${yajl}

yajl-clean:
ifneq ($(wildcard ext/yajl/Makefile})x,x)
	@${MAKE} -C ext/yajl distclean
endif
	@${RM} -f lib/libyajl.*
