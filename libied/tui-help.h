#if	!defined(_tui_help_h)
#define	_tui_help_h
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include "ringbuffer.h"

#ifdef __cplusplus
extern "C" {
#endif
    extern bool help_show(const char *help);
#ifdef __cplusplus
};
#endif
#endif	// !defined(_tui_help_h)
