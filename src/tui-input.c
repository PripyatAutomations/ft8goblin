//
// Keyboard/mouse input handler
//   
// XXX: We need to make keymaps a loadable thing, so each menu, pane, etc can select its own keymap
// XXX: This would make things a lot more pleasant for everyone!
// XXX: a lot of this code belongs in ft8goblin.c... someday it'll happen
//
#include "config.h"
#include "debuglog.h"
#include "tui.h"
#include "util.h"
#include "subproc.h"
#include "ft8goblin_types.h"
#include <ev.h>
#include <ctype.h>

static ev_io termbox_watcher, termbox_resize_watcher;
static ev_timer periodic_watcher;
extern TextArea *msgbox;
time_t now = 0;
//////
// These all are internal to tui-input and should probably end up static...
static const size_t input_buf_sz = TUI_INPUT_BUFSZ;		// really this is excessive even...
static size_t input_buf_cursor = 0, input_scrollback_lines = 0;
static char input_buf[TUI_INPUT_BUFSZ];
static rb_buffer_t *input_buf_history = NULL;
static rb_node_t   *input_buf_history_ptr = NULL;

void tui_show_input(void) {
   if (active_pane == PANE_INPUT) {
      printf_tb(0, line_input, TB_GREEN|TB_BOLD, TB_BLACK, "> %s", input_buf);
      tb_set_cursor(2 + input_buf_cursor, line_input);
   } else {
      printf_tb(0, line_input, TB_WHITE, TB_BLACK, "> %s", input_buf);
      tb_hide_cursor();
   }
   tb_present();
}

// Internal function to insert a character at the cursor
static void tui_insert_char(char c) {
   if (input_buf_cursor < input_buf_sz - 1) {
      memmove(&input_buf[input_buf_cursor + 1], &input_buf[input_buf_cursor], strlen(&input_buf[input_buf_cursor]) + 1);
      input_buf[input_buf_cursor] = c;
      input_buf_cursor++;
   }
   redraw_screen();
}

// Internal function to delete a character aat some offset from the cursor (usually 1 for DEL and -1 for BACKSPACE)
static void tui_delete_char(int offset) {
    if (offset > 0 && input_buf_cursor + offset <= strlen(input_buf)) {
        memmove(&input_buf[input_buf_cursor], &input_buf[input_buf_cursor + offset], strlen(&input_buf[input_buf_cursor + offset]) + 1);
    } else if (offset < 0 && input_buf_cursor + offset >= 0) {
        memmove(&input_buf[input_buf_cursor + offset], &input_buf[input_buf_cursor], strlen(&input_buf[input_buf_cursor]) + 1);
        input_buf_cursor += offset;
    }
    redraw_screen();
}

static void tui_clear_input_buf(void) {
   input_buf_cursor = 0;
   memset(input_buf, 0, input_buf_sz);
   redraw_screen();
}

static rb_node_t *tui_inputbuf_find_prev(void) {
   char *cp = NULL;
   rb_node_t *p = input_buf_history->head;
   rb_node_t *prev = NULL;

   while (p->next != NULL) {
      cp = (char *)p->data;

      if (cp == NULL) {
         fprintf(stderr, "tui_inputbuf_find_prev: data == NULL for node %p in rb <%p>", (void *)p, (void *)input_buf_history);
         return NULL;
      }

      if (strcmp(cp, input_buf) == 0) {
         log_send(mainlog, LOG_DEBUG, "tifp: match %p", p);
         return prev;
      }
      prev = p;
   }
   return NULL;
}

static rb_node_t *tui_inputbuf_find_next(void) {
   char *cp = NULL;
   rb_node_t *p = input_buf_history->head;
   while (p->next != NULL) {
      cp = (char *)p->data;

      if (cp == NULL) {
         fprintf(stderr, "tui_inputbuf_find_prev: data == NULL for node %p in rb <%p>", (void *)p, (void *)input_buf_history);
         return NULL;
      }

      if (strcmp(cp, input_buf) == 0) {
         log_send(mainlog, LOG_DEBUG, "tifp: match %p", p);
         return p;
      }
   }
   return NULL;
}

// XXX: This is the biggest offender of code that belongs in ft8goblin.c, not here!
void tui_process_input(struct tb_event *evt) {
   if (evt == NULL) {
      log_send(mainlog, LOG_CRIT, "process_input: called with ev == NULL. This shouldn't happen!");
      return;
   }

   if (evt->type == TB_EVENT_KEY) {
      // Is the key a valid command?
      if (evt->key == TB_KEY_ESC) {
         if (menu_level > 0) {
            menu_close();
         } else {
           menu_level = 0; // reset it to zero
         }
         return;
      } else if (evt->key == TB_KEY_BACKSPACE || evt->key == TB_KEY_CTRL_H || evt->key == 127) {
         if (active_pane == PANE_INPUT) {
            if (input_buf_cursor > 0) {
               tui_delete_char(-1);
            }
            return;
         } else {
            log_send(mainlog, LOG_DEBUG, "got backspace outside of input field, ignoring!");
         }
      } else if (evt->key == TB_KEY_CTRL_U) {
         if (active_pane == PANE_INPUT) {
            tui_clear_input_buf();
         }
      } else if (evt->key == TB_KEY_DELETE || evt->key == TB_KEY_CTRL_D) {
         if (active_pane == PANE_INPUT) {
            tui_delete_char(1);
            return;
         }
      } else if (evt->key == TB_KEY_HOME) {
         if (active_pane == PANE_INPUT) {
            input_buf_cursor = 0;
            redraw_screen();
         }
      } else if (evt->key == TB_KEY_END) {
         if (active_pane == PANE_INPUT) {
            input_buf_cursor = strlen(input_buf);
            redraw_screen();
         }
      } else if (evt->key == TB_KEY_F1) {
         // XXX: Figure out which help text to show based on active pain/window:
         //     main, msgs, scrollback, input
         help_show("main");
         return;
      } else if (evt->key == TB_KEY_F2) {		// Call CQ
         memset(input_buf, 0, input_buf_sz);
         char grid4[5];
         memcpy(grid4, gridsquare, 4);
         input_buf_cursor = snprintf(input_buf, input_buf_sz, "CQ %s %s", mycall, grid4);
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F3) {		// XXX: CALL MYCALL RXREPORT
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F4) { 	   	// XXX: CALL MYCALL R-RXREPORT
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F5) {		// XXX: CALL MYCALL RR73
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F6) {		// XXX: CALL MYCALL 73
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F6) {		// XXX: CALL MYCALL 73
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F7) {		// XXX:
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F8) {		// XXX:
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F9) {		// XXX:
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F10) {		// XXX:
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F11) {		// XXX:
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_F11) {		// XXX:
         //
         active_pane = PANE_INPUT;
         redraw_screen();
      } else if (evt->key == TB_KEY_TAB) {
        if (menu_level == 0) {		// only apply in main screen
           if (active_pane < PANE_INPUT) {		// go forward
              active_pane++;
           } else if (active_pane == PANE_INPUT) {	// wrap around
              active_pane = 0;
           }
        }
        redraw_screen();
      } else if (evt->key == TB_KEY_ENTER) {
         if (active_pane == PANE_INPUT) {
            // XXX: Send the message!
            char *mp = strndup(input_buf, TUI_INPUT_BUFSZ);
            if (mp == NULL) {
               fprintf(stderr, "process_input: out of memory!\n");
               exit(ENOMEM);
            }

            // add it to the history
            rb_add(input_buf_history, mp, true);

            log_send(mainlog, LOG_DEBUG, "We would send the message here... Msg: %s (%d)", input_buf, strlen(input_buf));

            // clear the buffer as it's been sent
            tui_clear_input_buf();
         }
      } else if (evt->key == TB_KEY_ARROW_LEFT) { 		// left cursor
         if (evt->mod == TB_MOD_CTRL) {
            log_send(mainlog, LOG_WARNING, "got theme_prev: Only theme available: default");
            // XXX: Go back a theme
            return;
         }
         if (active_pane == PANE_INPUT) {
            if (input_buf_cursor >= 1) {
               input_buf_cursor--;
            }
            redraw_screen();
         }
      } else if (evt->key == TB_KEY_ARROW_RIGHT) {		// right cursor
         if (evt->mod == TB_MOD_CTRL) {
            log_send(mainlog, LOG_WARNING, "got theme_next: Only theme available: default");
            // XXX: Go forward a theme
            return;
         }
         if (active_pane == PANE_INPUT) {
            // only applies in PANE_INPUT...
            if (input_buf_cursor < TUI_INPUT_BUFSZ) {
               // as long as this isnt end of line, continue... We want the NULL at end to be our cursor
               if (input_buf[input_buf_cursor] != '\0') {
                  input_buf_cursor++;
               }
            }
            redraw_screen();
         }
      } else if (evt->key == TB_KEY_ARROW_UP) {			// up cursor
         if (active_pane == PANE_INPUT) {
            // XXX: input scrollback
         }
      } else if (evt->key == TB_KEY_ARROW_DOWN) {		// down cursor
         if (active_pane == PANE_INPUT) {
            // XXX: input scrollback
         }
      } else if (evt->key == TB_KEY_CTRL_A) {			// ^A
         if (active_pane == PANE_INPUT) {
            input_buf_cursor = 0;
            redraw_screen();
         }
      } else if (evt->key == TB_KEY_CTRL_B) {			// ^B
         if (menu_level == 0) {					// only if we're at main TUI screen (not in a menu)
            menu_show(&menu_bands, 0);
         }
         return;
      } else if (evt->key == TB_KEY_CTRL_C) {			// ^C
         if (cq_only == false) {
            cq_only = true;
         } else {
            cq_only = false;
         }
         redraw_screen();
         log_send(mainlog, LOG_INFO, "Toggled CQ Only to %s", (cq_only ? "On" : "Off"));
         return;
      } else if (evt->key == TB_KEY_CTRL_E) {			// ^E
         input_buf_cursor = strlen(input_buf);
         redraw_screen();
      } else if (evt->key == TB_KEY_CTRL_O) {			// ^O
         toggle_tx_mode();
      } else if (evt->key == TB_KEY_CTRL_P) {			// ^P
         // toggle TX Even/Odd
         if (tx_even == false) {
            tx_even = true;
         } else {
            tx_even = false;
         }
         redraw_screen();
         log_send(mainlog, LOG_INFO, "Toggled TX slot to %s", (tx_even ? "EVEN" : "ODD"));
         return;
      } else if (evt->key == TB_KEY_CTRL_S) { 			// ^S
         if (menu_level == 0) {
            menu_history_clear();
            menu_show(&menu_main, 0);
         }
         return;
      } else if (evt->key == TB_KEY_CTRL_T) {			// ^T
         if (menu_level == 0) {
            if (tx_enabled == false) {
               tx_enabled = true;
            } else {
               tx_enabled = false;
            }
            redraw_screen();
         } else {
            // always disable if in a submenu, only allow activating TX from home screen
            tx_enabled = 0;
         }
         ta_printf(msgbox, "$RED$TX %sabled globally!", (tx_enabled ? "en" : "dis"));
         log_send(mainlog, LOG_NOTICE, "TX %sabled globally by user.", (tx_enabled ? "en" : "dis"));
         return;
      } else if (evt->key == TB_KEY_CTRL_W) {			// ^W
         halt_tx_now();
         return;
      } else if (evt->key == TB_KEY_CTRL_X || evt->key == TB_KEY_CTRL_Q) {	// is it ^X or ^Q? If so exit
         ta_printf(msgbox, "$RED$Goodbye! Hope you had a nice visit!");
         log_send(mainlog, LOG_NOTICE, "ft8goblin shutting down...");
         dying = 1;
         tui_shutdown();
         exit(0);
         return;
      } else if (evt->key == TB_KEY_CTRL_Y) {
         if (auto_cycle == false) {
            auto_cycle = true;
         } else {
            auto_cycle = false;
         }
         redraw_screen();
         return;
      } else {      					// Nope - display the event data for debugging
         if (active_pane != PANE_INPUT) {
            log_send(mainlog, LOG_DEBUG, "unknown tui-input event: type=%d key=%d ch=%c", evt->type, evt->key, evt->ch);
         }
      }

      // if we make it here, then this input has fallen through and should get added to input buffer (probably)
      if (active_pane == PANE_INPUT) {
         // but only if it's printable...
         if (isprint(evt->ch)) {
            char c = evt->ch;

            // these modes only allow upper case
            if (tx_mode == TX_MODE_FT4 || tx_mode == TX_MODE_FT8) {
               tui_insert_char(toupper(c));
            } else {
               tui_insert_char(c);
            }
//            log_send(mainlog, LOG_DEBUG, "appending to %c to input_buf <%p>: %s at %lu", c, input_buf, input_buf, input_buf_cursor);
            redraw_screen();
         }
      }
   } else if (evt->type == TB_EVENT_RESIZE) {
      // change the stored dimensions/layout variables above
      tui_resize_window(evt);

      // clear the screen buffer
      tb_clear();

      // redraw the various sections of the screen
      redraw_screen();
   } else if (evt->type == TB_EVENT_MOUSE) {     // handle mouse interactions
      if (evt->key == TB_KEY_MOUSE_LEFT) {
         // Figure out if it's within the borders of a pane and if so, switch to that pane.
         if (evt->y > 3 && evt->y < line_input) {
            // OK it's not inside the help or status lines, it's in the TextArea region, figure out which pane
            if (evt->x >= ((width / 2) + 1)) {     // it's the right pane
               active_pane = PANE_LOOKUP;
            } else {			        // it's the left pane
               active_pane = PANE_MSGS;
            }
         } else if (evt->y == line_input) {	// its in the input pane
            active_pane = PANE_INPUT;
         }
         redraw_screen();
      } else if (evt->key == TB_KEY_MOUSE_RIGHT) {
         // XXX: Allow right clicks for a menu someday
      } else if (evt->key == TB_KEY_MOUSE_WHEEL_UP) {	// scroll up
        if (active_pane == PANE_MSGS) {
           // scroll the text area
           log_send(mainlog, LOG_DEBUG, "scroll up in msgs");
        } else if (active_pane == PANE_LOOKUP) {
           log_send(mainlog, LOG_DEBUG, "scroll up in lookup");
        }
      } else if (evt->key == TB_KEY_MOUSE_WHEEL_DOWN) {	// scroll down
        if (active_pane == PANE_MSGS) {
           // scroll the text area
           log_send(mainlog, LOG_DEBUG, "scroll down in msgs");
        } else if (active_pane == PANE_LOOKUP) {
           log_send(mainlog, LOG_DEBUG, "scroll down in lookup");
        }
      }
      log_send(mainlog, LOG_DEBUG, "mouse <x,y> = <%d,%d>", evt->x, evt->y);
   }
   tb_present();
}

//////////////////////////////////////////
// Handle termbox tty and resize events //
//////////////////////////////////////////
static void termbox_cb(EV_P_ ev_io *w, int revents) {
   struct tb_event evt;		// termbox io events
   tb_poll_event(&evt);
   tui_process_input(&evt);
}

// XXX: this belongs in the user code and passed as part of
// XXX: setup. Please fix this!
static void periodic_cb(EV_P_ ev_timer *w, int revents) {
   now = time(NULL);
   subproc_check_all();
   redraw_screen();
   // XXX: Handle RX and TX timers
}

int tui_io_watcher_init(void) {
   struct ev_loop *loop = EV_DEFAULT;
   int rv = 0;
   int fd_tb_tty = -1, fd_tb_resize = -1;

   ///////////////////////////////////////////
   // Setup libev to handle termbox2 events //
   ///////////////////////////////////////////
   tb_get_fds(&fd_tb_tty, &fd_tb_resize);

   // stdio occupy 0-2 (stdin, stdout, stderr)
   if (fd_tb_tty >= 2 && fd_tb_resize >= 2) {
      // add to libev set
   } else {
      ta_printf(msgbox, "$RED$tb_get_fds returned nonsense (%d, %d) can't continue!", fd_tb_tty, fd_tb_resize);
      log_send(mainlog, LOG_CRIT, "tb_get_fds returned nonsense (%d, %d) - I can't continue!", fd_tb_tty, fd_tb_resize);
      tb_present();
      exit(200);
   }

   // start watchers on the termbox events
   ev_io_init(&termbox_watcher, termbox_cb, fd_tb_tty, EV_READ);
   ev_io_init(&termbox_resize_watcher, termbox_cb, fd_tb_resize, EV_READ);
   ev_io_start(loop, &termbox_watcher);
   ev_io_start(loop, &termbox_resize_watcher);

   // start our once a second periodic timer (used for housekeeping and the clock display)
   ev_timer_init(&periodic_watcher, periodic_cb, 0, 1);
   ev_timer_start(loop, &periodic_watcher);

   return rv;
}

void tui_input_init(void) {
   memset(input_buf, 0, input_buf_sz);
   if (input_buf_history == NULL) {
      input_scrollback_lines = cfg_get_int(cfg, "ui/input-history-lines");
      input_buf_history = rb_create(input_scrollback_lines, "input_sb");
   }
}

void tui_input_shutdown(void) {
   /// ...
}
