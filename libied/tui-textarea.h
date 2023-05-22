#if	!defined(_tui_textarea_h)
#define	_tui_textarea_h
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include "ringbuffer.h"

#define	MAX_TEXTAREAS	6

#ifdef __cplusplus
extern "C" {
#endif
   typedef struct TextArea {
      int	scrollback_lines;	// this is set in main below...
      int	selected_line;
      rb_buffer_t* scrollback;		// the ring buffer for scrollback
      int 	top,			// top of scrollable TextArea
                bottom;			// bottom of scrollable TextArea
      int	x_offset;		// offset from left edge
      char	name[65];		// a descriptive name of the textarea
   } TextArea;
   // Handling of the TextArea (roughly in order of lifecycle)
   extern TextArea *ta_init(const char *name, int scrollback_lines);
   extern int ta_append(TextArea *ta, const char *buf);
   extern void ta_printf(TextArea *ta, const char *fmt, ...);
   extern void ta_redraw(TextArea *ta);
   extern void ta_resize(TextArea *ta);
   extern void ta_redraw_all(void);
   extern void ta_resize_all(void);
   extern void ta_destroy(TextArea *ta);
   extern int scrollback_lines;

#ifdef __cplusplus
};
#endif
#endif	// !defined(_tui_textarea_h)
