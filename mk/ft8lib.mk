###########
# ft8_lib #
###########
ft8lib := lib/libft8.so
ft8lib_hdrs += ext/ft8_lib/ft8/constants.h
ft8lib_hdrs += ext/ft8_lib/ft8/crc.h
ft8lib_hdrs += ext/ft8_lib/ft8/decode.h
ft8lib_hdrs += ext/ft8_lib/ft8/encode.h
ft8lib_hdrs += ext/ft8_lib/ft8/ldpc.h
ft8lib_hdrs += ext/ft8_lib/ft8/pack.h
ft8lib_hdrs += ext/ft8_lib/ft8/text.h
ft8lib_hdrs += ext/ft8_lib/ft8/unpack.h

ft8lib_objs += obj/ft8lib/common/wave.o
ft8lib_objs += obj/ft8lib/fft/kiss_fft.o
ft8lib_objs += obj/ft8lib/fft/kiss_fftr.o
ft8lib_objs += obj/ft8lib/ft8/constants.o
ft8lib_objs += obj/ft8lib/ft8/crc.o
ft8lib_objs += obj/ft8lib/ft8/decode.o
ft8lib_objs += obj/ft8lib/ft8/encode.o
ft8lib_objs += obj/ft8lib/ft8/text.o
ft8lib_objs += obj/ft8lib/ft8/pack.o
ft8lib_objs += obj/ft8lib/ft8/ldpc.o
ft8lib_objs += obj/ft8lib/ft8/unpack.o

${ft8lib}: ${ft8lib_objs}
	@echo "[SO] $^ -> $@"
	${CC} ${LDFLAGS} -fPIC -shared -Wl,-soname,$@ -lm -o $@ $^

obj/ft8lib/common/%.o: ext/ft8_lib/common/%.c
	@echo "[CC] $< -> $@"
	@${CC} ${CFLAGS} ${ft8lib_cflags} -o $@ -c $<

obj/ft8lib/fft/%.o: ext/ft8_lib/fft/%.c
	@echo "[CC] $< -> $@"
	@${CC} ${CFLAGS} ${ft8lib_cflags} -o $@ -c $<

obj/ft8lib/ft8/%.o: ext/ft8_lib/ft8/%.c
	@echo "[CC] $< -> $@"
	@${CC} ${CFLAGS} ${ft8lib_cflags} -o $@ -c $<

ft8lib-clean:
	@${RM} -f ${ft8lib_objs} ${ft8lib}

extra_build_targets += ${ft8lib}
extra_clean_targets += ft8lib-clean
