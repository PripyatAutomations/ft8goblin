//
// Keyboard/mouse input handler
//   
// XXX: We need to make keymaps a loadable thing, so each menu, pane, etc can select its own keymap
// XXX: This would make things a lot more pleasant for everyone!
// XXX: a lot of this code belongs in ft8goblin.c... someday it'll happen
#include "config.h"
#include "debuglog.h"
#include "tui.h"
#include "util.h"
#include "subproc.h"
#include "ft8goblin_types.h"
#include <ev.h>

static ev_io termbox_watcher, termbox_resize_watcher;
static ev_timer periodic_watcher;
extern TextArea *msgbox;
time_t now = 0;

// XXX: This is the biggest offender of code that belongs in ft8goblin.c, not here!
void process_input(struct tb_event *evt) {
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
      } else if (evt->key == TB_KEY_TAB) {
        if (menu_level == 0) {		// only apply in main screen
           if (active_pane == 0)
              active_pane = 1;
           else
              active_pane = 0;
        }
      } else if (evt->key == TB_KEY_ARROW_LEFT) { 		// left cursor
      } else if (evt->key == TB_KEY_ARROW_RIGHT) {		// right cursor
      } else if (evt->key == TB_KEY_ARROW_UP) {			// up cursor
      } else if (evt->key == TB_KEY_ARROW_DOWN) {		// down cursor
      } else if (evt->key == TB_KEY_CTRL_B) {			// ^B
         if (menu_level == 0) {					// only if we're at main TUI screen (not in a menu)
            menu_show(&menu_bands, 0);
         }
      } else if (evt->key == TB_KEY_CTRL_E) {			// ^E
         // toggle TX Even/Odd
         if (tx_even == false) {
            tx_even = true;
         } else {
            tx_even = false;
         }
         redraw_screen();
         log_send(mainlog, LOG_INFO, "Toggled to %s TX slot!", (tx_even ? "EVEN" : "ODD"));
      } else if (evt->key == TB_KEY_CTRL_H) {			// ^H
         halt_tx_now();
      } else if (evt->key == TB_KEY_CTRL_S) { 			// Is it ^S?
         if (menu_level == 0) {
            menu_history_clear();
            menu_show(&menu_main, 0);
         } else {
            // pass ^M through
         }
      } else if (evt->key == TB_KEY_CTRL_T) {		// ^T
         if (menu_level == 0) {
            toggle(&tx_enabled);
            redraw_screen();
         } else {
            // always disable if in a submenu, only allow activating TX from home screen
            tx_enabled = 0;
         }
         ta_printf(msgbox, "$RED$TX %sabled globally!", (tx_enabled ? "en" : "dis"));
         log_send(mainlog, LOG_NOTICE, "TX %sabled globally by user.", (tx_enabled ? "en" : "dis"));
      } else if (evt->key == TB_KEY_CTRL_X || evt->key == TB_KEY_CTRL_Q) {	// is it ^X or ^Q? If so exit
         ta_printf(msgbox, "$RED$Goodbye! Hope you had a nice visit!");
         log_send(mainlog, LOG_NOTICE, "ft8goblin shutting down...");
         dying = 1;
         tui_shutdown();
         exit(0);
         return;
      } else {      					// Nope - display the event data for debugging
         ta_printf(msgbox, "$RED$unknown event: type=%d key=%d ch=%c", evt->type, evt->key, evt->ch);
         log_send(mainlog, LOG_DEBUG, "unknown event: type=%d key=%d ch=%c", evt->type, evt->key, evt->ch);
      }
   } else if (evt->type == TB_EVENT_RESIZE) {
      // change the stored dimensions/layout variables above
      tui_resize_window();

      // clear the screen buffer
      tb_clear();

      // redraw the various sections of the screen
      redraw_screen();
   } else if (evt->type == TB_EVENT_MOUSE) {
      // handle mouse interactions
   }
   tb_present();
}

//////////////////////////////////////////
// Handle termbox tty and resize events //
//////////////////////////////////////////
static void termbox_cb(EV_P_ ev_io *w, int revents) {
   struct tb_event evt;		// termbox io events
   tb_poll_event(&evt);
   process_input(&evt);
}

// XXX: this belongs in the user code and passed as part of
// XXX: setup. Please fix this!
static void periodic_cb(EV_P_ ev_timer *w, int revents) {
   now = time(NULL);
   subproc_check_all();
   redraw_screen();
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
