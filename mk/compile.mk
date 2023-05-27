obj/%.o: src/%.c $(wildcard include/*.h)
	@echo "[CC] $< -> $@"
	@${CC} ${CFLAGS} -o $@ -c $<

obj/%.o: src/%.cc $(wildcard include/*.h)
	@echo "[CXX] $< -> $@"
	@${CXX} ${CXXFLAGS} -o $@ -c $<
