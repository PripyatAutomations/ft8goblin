#if	!defined(_tui_input_h)
#define	_tui_input_h
#include <termbox2.h>

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
      uint8_t type;
      uint8_t mod;
      uint16_t key;
      uint32_t ch;
      void (*callback)();
   } Keymap;

   extern void process_input(struct tb_event *evt);
   extern int tui_io_watcher_init(void);
   extern void tui_input_init(void);
   extern void tui_input_shutdown(void);
   extern struct ev_loop *loop;
   extern time_t now;
   extern const size_t input_buf_sz;
   extern size_t input_buf_offset;
   extern size_t input_buf_cursor;
   extern char input_buf[TUI_INPUT_BUFSZ];
   ///// ft8goblin.c
   extern const char *mycall;      // cfg:ui/mycall
   extern const char *gridsquare;  // cfg:ui/gridsquare
   extern bool auto_cycle;

#ifdef __cplusplus
};
#endif

#endif	// !defined(_tui_input_h)
