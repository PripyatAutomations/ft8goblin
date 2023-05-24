callsign_lookup := bin/callsign-lookup

${callsign_lookup}: ext/callsign-lookup/bin/callsign-lookup
	@install -m 0755 $< $@

# try to trigger rebuilds if changes
ext/callsign-lookup/bin/callsign-lookup: $(wildcard ext/callsign-lookup/include/*.h ext/callsign-lookup/src/*.c ext/callsign-lookup/mk/*) etc/calldata-cache.db ext/callsign-lookup/GNUmakefile
	${MAKE} -C ext/callsign-lookup world

extra_clean_targets += callsign-lookup-clean

callsign-lookup-clean:
	${MAKE} -C ext/callsign-lookup distclean
	${RM} bin/callsign-lookup

etc/calldata-cache.db:
	[ ! -f $@ ] && sqlite3 etc/calldata-cache.db < sql/cache.sql 

.PHONY: bin/callsign-lookup

callsign-lookup-install: bin/callsign-lookup
	install -m 0755 bin/callsign-lookup ${bin_install_path}

extra_install_targets += callsign-lookup-install
