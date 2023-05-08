/*
 * This needs much improvement and a lot of src/tui-textarea.c belongs as a keymap here
 */
#include "config.h"
#include "subproc.h"
#include "util.h"
#include "tui.h"
#include "debuglog.h"
#include "watch.h"
#include "daemon.h"
#include "qrz-xml.h"
#include "ft8goblin_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <ev.h>
#include <evutil.h>
#include <termbox2.h>

char *progname = "ft8goblin";
TextArea *msgbox = NULL;
const char *mycall = NULL;	// cfg:ui/mycall
const char *gridsquare = NULL;	// cfg:ui/gridsquare
int	dying = 0;		// Are we shutting down?
int	tx_enabled = 0;		// Master toggle to TX mode.

int	line_status = -1;		// status line
int 	line_input = -1;		// input field
int	height = -1, width = -1;
int	active_band = 40;		// Which band are we TXing on?
bool	tx_even = false;		// TX even or odd cycle?
int	active_pane = 1;		// active pane (0: TextArea, 1: TX input)

static void exit_fix_config(void) {
   printf("Please edit your config.json and try again!\n");
   exit(255);
}

static void print_help(void) {
   int offset = 0;
   printf_tb(offset, 0, TB_GREEN|TB_BOLD, 0, "*Keys* ");
   offset += 7;
   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^Q/^X ");
   offset += 6;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Exit ");
   offset += 5;
   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "ESC ");
   offset += 4;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Go Back ");
   offset += 8;

   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "TAB ");
   offset += 4;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Switch Pane ");
   offset += 12;
   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^H ");
   offset += 3;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Halt TX now ");
   offset += 12;
   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^S ");
   offset += 3;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Settings ");
   offset += 9;
   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^T ");
   offset += 3;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Toggle TX ");
   offset += 10;
   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^E ");
   offset += 3;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Even/Odd");
   offset += 10;
   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^B ");
   offset += 3;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Bands ");
   offset += 6;
}

static void print_status(void) {
   int offset = 0;

   // time
   char outstr[32];
   time_t t;
   struct tm *tmp;
   memset(outstr, 0, 32);

   if ((tmp = localtime(&now)) == NULL) {
       perror("localtime");
       exit(EXIT_FAILURE);
   }

   if (strftime(outstr, sizeof(outstr), "%H:%M:%S", tmp) == 0) {
      log_send(mainlog, LOG_CRIT, "strftime returned 0");
      exit(EXIT_FAILURE);
   }

   printf_tb(offset, height - 1, TB_YELLOW|TB_BOLD, 0, "%s ", outstr);
   offset += 9;

   // callsign
   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "[Oper:");
   offset += 6;
   printf_tb(offset, height - 1, TB_CYAN|TB_BOLD, 0, "%s", mycall);
   offset += strlen(mycall);
   printf_tb(offset++, height - 1, TB_RED|TB_BOLD, 0, "@");
   printf_tb(offset, height - 1, TB_CYAN|TB_BOLD, 0, "%s", gridsquare);
   offset += strlen(gridsquare);
   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;

   // TX enabled status
   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "[");
   offset++;
   printf_tb(offset, height - 1, TB_GREEN|TB_BOLD, 0, "TX:");
   offset += 3;

   if (tx_enabled) {
      printf_tb(offset, height - 1, TB_RED|TB_BOLD, 0, "ON");
      offset += 2;
   } else {
      printf_tb(offset, height - 1, TB_GREEN|TB_BOLD, 0, "OFF");
      offset += 3;
   }
   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;

   // show bands with TX enabled, from yajl tree...
   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "[");
   offset++;
   printf_tb(offset, height - 1, TB_GREEN|TB_BOLD, 0, "TXBand:");
   offset += 7;

   if (active_band != 0) {
      printf_tb(offset, height - 1, TB_RED|TB_BOLD, 0, "%dm", active_band);
      offset += 3;

      printf_tb(offset++, height - 1, TB_WHITE|TB_BOLD, 0, "/");

      if (tx_even) {
         printf_tb(offset, height - 1, TB_YELLOW|TB_BOLD, 0, "EVEN");
         offset += 4;
      } else {
         printf_tb(offset, height - 1, TB_YELLOW|TB_BOLD, 0, "ODD");
         offset += 3;
      }
   }

   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;

   // print the PTT status
   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "[");
   offset++;
   printf_tb(offset, height - 1, TB_GREEN|TB_BOLD, 0, "PTT:");
   offset += 4;
#if	0
   // Explode the list of radios actively PTTing
   for (int i = 0; i < max_rigs; i++) {
      if (rigs[i].ptt_active) {
         printf_tb(offset, height - 1, TB_RED|TB_BOLD, 0, "%d", i);
      }
   }
#endif
   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;
   tb_present();
}

static void print_input(void) {
   // XXX: Draw the input text area
}

void redraw_screen(void) {
   // Show help (keys)
   print_help();
   // redraw all TextAreas
   ta_redraw_all();
   // print the input prompt
   print_input();
   // and the status line
   print_status();
   // the render it!
   tb_present();
}

int main(int argc, char **argv) {
   struct ev_loop *loop = EV_DEFAULT;

   // print this even though noone will see it, except in case of error exit ;)
   printf("ft8goblin: A console based ft8 client with support for multiband operation\n\n");

   // This can't work without a valid configuration...
   if (!(cfg = load_config()))
      exit_fix_config();

   // setup some values that are frequently requested
   mycall = cfg_get_str(cfg, "site/mycall");
   gridsquare = cfg_get_str(cfg, "site/gridsquare");

   const char *logpath = dict_get(runtime_cfg, "logpath", "file://ft8goblin.log");
   if (logpath != NULL) {
      mainlog = log_open(logpath);
   } else {
      log_open("stderr");
   }
   log_send(mainlog, LOG_NOTICE, "ft8goblin starting up!");

   tui_init();
   // create the default TextArea for messages
   msgbox = ta_init(cfg_get_int(cfg, "ui/scrollback-lines"));
   tui_resize_window(NULL);
   tui_io_watcher_init();
   ta_printf(msgbox, "$CYAN$Welcome to ft8goblin, a console ft8 client with support for multiple bands!");

   // Draw the initial screen
   redraw_screen();

   // Load the watchlists
   watchlist_load(cfg_get_str(cfg, "ui/alerts/watchfile"));

   // Set up IPC

   // Start supervising subprocesses:
   //	ft8capture (single instance per device)
        // XXX: Walk the tree at cfg:devices
        // XXX: Walk the tree at cfg:bands
   //	ft8decoder (one per band)
   // 	callsign-lookupd (one thread)
//        subproc_start()

   // main loop...
   while (!dying) {
      // XXX: do libev stuff and decide if there's waiting input on fd_tb_tty or fd_fb_resize and act appropriately...
      ev_run (loop, 0);

      // if ev loop exits, we need to die..
      dying = 1;
   }

   tui_shutdown();
   log_close(mainlog);
   mainlog = NULL;
   fini(0);			// remove pidfile, etc
   return 0;
}

///////////
// Menus //
///////////
int view_config(void) {
   // XXX: Show the yajl config tree as a scrollable 'menu', without editing
   return 0;
}

void halt_tx_now(void) {
   ta_printf(msgbox, "$RED$Halting TX!");
   tx_enabled = false;
   log_send(mainlog, LOG_CRIT, "Halting TX immediately at user request!");
   redraw_screen();
   tb_present();
}
