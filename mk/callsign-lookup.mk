callsign_lookup := bin/callsign-lookup

${callsign_lookup}: ext/callsign-lookup/bin/callsign-lookup
	install -m 0755 $< $@

ext/callsign-lookup/bin/callsign-lookup: etc/calldata-cache.db
	${MAKE} -C ext/callsign-lookup world

extra_clean_targets += callsign-lookup-clean

callsign-lookup-clean:
	${MAKE} -C ext/callsign-lookup distclean
	${RM} bin/callsign-lookup

etc/calldata-cache.db:
	[ ! -f $@ ] && sqlite3 etc/calldata-cache.db < sql/cache.sql 
