#if	!defined(_tui_h)
#define	_tui_h
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include "ringbuffer.h"
#include "tui-menu.h"
#include "tui-input.h"
#include "tui-textarea.h"
#include "tui-help.h"

#define	MAX_TEXTAREAS	6

#ifdef __cplusplus
extern "C" {
#endif
   typedef struct ColorPair {
      int fg, bg;
   } ColorPair;
   extern ColorPair parse_color_str(const char *str);
   extern int parse_color(const char *str);

   // Print *anywhere* on the screen (dont try to print on the TextAreas ;)
   extern void print_tb(const char *str, int x, int y, uint16_t fg, uint16_t bg);
   extern void printf_tb(int x, int y, uint16_t fg, uint16_t bg, const char *fmt, ...);

   // Returns: index of button pressed. (Buttons are 0 = OK, 1 = CANCEL, 2 = OK|CANCEL, 3 = OK|CANCEL|HELP)
   extern int modal_dialog(int buttons, int border_fg, int border_bg, int title_fg, int title_bg, const char *title, int text_fg, int text_bg, const char *fmt, ...);

   // ui functions
   extern void tui_init(void (*cb)());
   extern void tui_resize_window(struct tb_event *evt);
   extern void tui_shutdown(void);
   extern void tui_redraw(void);
   extern time_t now;
   extern bool dying;
   extern int active_band;		// Which band are we TXing on?
   extern int active_pane;		// active pane (0: TextArea, 1: TX input)
   extern int height, width;
   extern int line_status;		// status line
   extern int line_input;		// input field
   extern TextArea *msgbox;
#ifdef __cplusplus
};
#endif
#endif	// !defined(_tui_h)
