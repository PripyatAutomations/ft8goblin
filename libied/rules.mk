libied_objs += adif.o	# ADIF logs
libied_objs += cfg.o
libied_objs += daemon.o
libied_objs += debuglog.o
libied_objs += dict.o
libied_objs += ipc.o
libied_objs += maidenhead.o	# maidenhead coordinate tools
libied_objs += memory.o
libied_objs += ringbuffer.o
libied_objs += sql.o		# sqlite3 / postgis wrapper
libied_objs += subproc.o	# subprocess management
libied_objs += util.o
libied_objs += tui.o
libied_objs += tui-help.o
# XXX: This is too intertwined into ft8goblin right now
#libied_objs += tui-input.o
libied_objs += tui-keymap.o
libied_objs += tui-menu.o
libied_objs += tui-textarea.o

####################################################################
# ugly hacks to quiet the compiler until we can clean things up... #
####################################################################
obj/tui-menu.o: src/tui-menu.c
	@echo "[CC] $< -> $@"
#	${CC} $(filter-out -Werror,${CFLAGS}) -o $@ -c $<
	@${CC} ${CFLAGS} -o $@ -c $< -Wno-error=int-conversion -Wno-missing-braces
