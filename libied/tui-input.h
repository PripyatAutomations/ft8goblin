#if	!defined(_tui_input_h)
#define	_tui_input_h
#include <termbox2/termbox2.h>
#include "tui-menu.h"

#define	MAX_KEY_ID	0xffff
#define	TUI_INPUT_BUFSZ		512
#define	PANE_MSGS		0
#define	PANE_LOOKUP		1
#define	PANE_INPUT		2

#ifdef __cplusplus
extern "C" {
#endif
   // make a keymap out of these? ;)
   typedef struct Keymap {
      // termbox2 event data
      uint8_t type;
      uint8_t mod;
      uint16_t key;
      uint32_t ch;
      // filters
      int active_pane;		// allow to be active only in some panes
      int menu_level;		// allow to only occur in certain menu levels (0 or 1 usually)
      char menu_name[MAX_MENUNAME];       
      // callback when key pressed
      void (*callback)();
   } Keymap;

   extern void tui_process_input(struct tb_event *evt);
   extern int tui_io_watcher_init(void);
   extern void tui_show_input(void);
   extern void tui_input_init(void);
   extern void tui_input_shutdown(void);
   extern time_t now;
//   extern const size_t input_buf_sz;
//   extern size_t input_buf_offset;
//   extern size_t input_buf_cursor;
//   extern char input_buf[TUI_INPUT_BUFSZ];
#ifdef __cplusplus
};
#endif

#endif	// !defined(_tui_input_h)
