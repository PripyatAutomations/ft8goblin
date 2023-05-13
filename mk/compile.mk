obj/%.o: src/%.c $(wildcard include/*.h)
	@echo "[CC] $< -> $@"
	@${CC} ${CFLAGS} -o $@ -c $<
