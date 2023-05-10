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
#include <time.h>
#include <ev.h>
#include <evutil.h>
#include <termbox2.h>

#define	MIN_HEIGHT	25
#define	MIN_WIDTH	80

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
bool	cq_only = false;		// Only show CQ & active QSOs?
int	active_pane = 0;		// active pane (0: TextArea, 1: TX input)
rb_buffer_t *callsign_lookup_history = NULL;
size_t callsign_lookup_history_sz = 1;

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

   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "PGUP/PGDN ");
   offset += 10;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Scroll ");
   offset += 7;

   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "TAB ");
   offset += 4;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Switch Pane ");
   offset += 12;

   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^H ");
   offset += 3;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Halt TX now ");
   offset += 12;

   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^T ");
   offset += 3;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Toggle TX ");
   offset += 10;

   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^E ");
   offset += 3;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Even/Odd");
   offset = 7;

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "^1 - ^0 ");
   offset += 8;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "Automsg ");
   offset += 8;

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "^C ");
   offset += 3;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "CQ Only ");
   offset += 8;

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "^L ");
   offset += 3;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "Lookup ");
   offset += 7;

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "^F ");
   offset += 3;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "Filters ");
   offset += 8;

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "^B ");
   offset += 3;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "Bands ");
   offset += 6;

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "^S ");
   offset += 3;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "Settings ");
   offset += 9;
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

   // only show CQ and active QSOs?
   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "[");
   offset++;
   printf_tb(offset, height - 1, TB_CYAN, 0, "CQonly:");
   offset += 7;

   if (cq_only) {
      printf_tb(offset, height - 1, TB_RED|TB_BOLD, 0, "ON");
      offset += 2;
   } else {
      printf_tb(offset, height - 1, TB_GREEN|TB_BOLD, 0, "OFF");
      offset += 3;
   }
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
#if	0
   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "[");
   offset++;
   printf_tb(offset, height - 1, TB_GREEN|TB_BOLD, 0, "PTT:");
   offset += 4;
   // Explode the list of radios actively PTTing
   for (int i = 0; i < max_rigs; i++) {
      if (rigs[i].ptt_active) {
         printf_tb(offset, height - 1, TB_RED|TB_BOLD, 0, "%d", i);
      }
   }
   printf_tb(offset, height - 1, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;
#endif

   char verbuf[128];
   memset(verbuf, 0, 128);
   snprintf(verbuf, 128, "ft8goblin/%s", VERSION);
   size_t ver_len = strlen(verbuf);
   printf_tb(tb_width() - ver_len, height - 1, TB_MAGENTA|TB_BOLD, 0, "%s", verbuf);
   tb_present();
}

static void print_input(void) {
   int x = 0,
       y = height - 2;
   if (active_pane == 2) {
      printf_tb(x, y, TB_GREEN|TB_BOLD, TB_BLACK, "> ");
      tb_set_cursor(x + 2, y);
   } else {
      printf_tb(x, y, TB_WHITE, TB_BLACK, "> ");
      tb_hide_cursor();
   }
   tb_present();
}

// Renders the data passed in a qrz_callsign_t to a dialog window
void render_call_lookup(qrz_callsign_t *calldata) {
   int x = (width / 2) + 1,
       y = 2;

   if (calldata == NULL) {
      // error handling!
      log_send(mainlog, LOG_WARNING, "render_call_lookup passed NULL calldata");
      return;
   }

   // print callsign data
   int WIN_BORDER_FG = -1;
   int WIN_BORDER_BG = -1;
   char padbuf[256];
   size_t padlen = (width / 2) - 3;
   memset(padbuf, 0, padlen);
   for (size_t i = 0; i < padlen; i++) {
      padbuf[i] = '-';
   }

   if (active_pane == 1) {
      WIN_BORDER_FG = TB_GREEN;
      WIN_BORDER_BG = TB_BLACK;
   } else {
      WIN_BORDER_FG = TB_WHITE;
      WIN_BORDER_BG = TB_BLACK;
   }
   printf_tb(x, y++, WIN_BORDER_FG, WIN_BORDER_BG, "┌%s┐", padbuf);
   x += 1;
   printf_tb(x, y++, TB_RED|TB_BOLD, TB_BLACK, "Callsign %s (Active QSO)", calldata->callsign);
   printf_tb(x, y++, TB_CYAN|TB_BOLD, TB_BLACK, "DXCC %s Grid %s = %3.3f,%3.3f", calldata->dxcc, calldata->grid,
      calldata->latitude, calldata->longitude);

   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Distance: 1102 mi, heading 293°");
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Lic. Class: General (Vanity)");
   printf_tb(x, y++, TB_RED|TB_BOLD, TB_BLACK, "Prev. Calls: %s", calldata->previous_call);
   // XXX: Add days til expires?

   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Expires: 2028/03/01");
   printf_tb(x, y++, TB_YELLOW|TB_BOLD, TB_BLACK, "Logbook QSOs: 3 (2 bands)");
   // skip a line
   y++;
   printf_tb(x, y++, TB_GREEN|TB_BOLD, TB_BLACK, "Robert Test");
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "123 Sesame St");
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Earth City, MO 64153 USA");
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Email: bobtest@test.com");
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Phone: 987-654-3210");
   printf_tb(x, y++, TB_WHITE, TB_BLACK, "URL: https://qrz.com/db/AA1AB");
   printf_tb(x, y++, TB_MAGENTA, TB_BLACK, "Data sourced from QRZ (11 hours old)");
   x -= 1;
   printf_tb(x, height - 3, WIN_BORDER_FG, WIN_BORDER_BG, "└%s┘", padbuf);
}

qrz_callsign_t *fake_q;

void draw_fake_ta(void) {
   char datebuf[128];

   time_t now = time(NULL);
   struct tm *tm = NULL;
   tm = localtime(&now);
   memset(datebuf, 0, 128);
   strftime(datebuf, 128, "%H:%M:15", tm);
   int x = 0,
       y = 2;

   // print callsign data
   int WIN_BORDER_FG = -1;
   int WIN_BORDER_BG = -1;
   char padbuf[width/2];
   size_t padlen = (width / 2) - 1;
   memset(padbuf, 0, padlen);

   for (size_t i = 0; i < padlen; i++) {
      padbuf[i] = '-';
   }

   if (active_pane == 0) {
      WIN_BORDER_FG = TB_GREEN;
      WIN_BORDER_BG = TB_BLACK;
   } else {
      WIN_BORDER_FG = TB_WHITE;
      WIN_BORDER_BG = TB_BLACK;
   }
   printf_tb(x, y++, WIN_BORDER_FG, WIN_BORDER_BG, "┌%s┐", padbuf);
   x += 1;


   printf_tb(x, y, TB_WHITE, TB_BLACK, "%s", datebuf);
   printf_tb(x + strlen(datebuf) + 1, y++, TB_BLACK|TB_BOLD, TB_YELLOW, "[ft8-40m-1]  TX CQ N0CALL AA12 +1200");

   localtime(&now);
   memset(datebuf, 0, 128);
   strftime(datebuf, 128, "%H:%M:30", tm);

   printf_tb(x, y, TB_WHITE|TB_WHITE, TB_BLACK, "%s", datebuf);
   printf_tb(x + strlen(datebuf) + 1, y++, TB_WHITE, TB_CYAN, "[ft8-20m-1] -12 CQ KT3ST EM20 +670");

   printf_tb(x, y, TB_WHITE, TB_BLACK, "%s", datebuf);
   x += strlen(datebuf) + 1;
   char buf[512];
   memset(buf, 0, 512);
   snprintf(buf, 512, "[ft8-40m-1]  +6 N0CALL AA1AB EM32 +320");
   printf_tb(x, y, TB_WHITE|TB_BOLD, TB_RED, "%s", buf);
   x += strlen(buf);
   printf_tb(x, y++, TB_BLACK, TB_RED, " *QSO*");

   x = 1;
   printf_tb(x, y, TB_WHITE, TB_BLACK, "%s", datebuf);
   x += strlen(datebuf) + 1;
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_CYAN, "[ft8-6m-1]   -3 CQ KD3ABC EN30 +550");
   x = 0;
   printf_tb(x, height - 3, WIN_BORDER_FG, WIN_BORDER_BG, "└%s┘", padbuf);

   // XXX: In the real code, we'll need to search the RingBuffer for our pointer.
   if (fake_q == NULL) {
      // don't manually free this as it will be freed by the RingBuffer code!
      if ((fake_q = malloc(sizeof(qrz_callsign_t))) == NULL) {
         fprintf(stderr, "draw_fake_ta: out of memory!\n");
         exit(ENOMEM);
      }
      
      memset(fake_q, 0, sizeof(qrz_callsign_t));
      memset(fake_q, 0, sizeof(qrz_callsign_t));
      snprintf(fake_q->callsign, sizeof(fake_q->callsign), "AA1AB");
      snprintf(fake_q->dxcc, sizeof(fake_q->dxcc), "291");
      snprintf(fake_q->grid, MAX_GRID_LEN, "EM32");
      snprintf(fake_q->previous_call, MAX_CALLSIGN, "N0FUX");
      fake_q->latitude = 32.021;
      fake_q->longitude = -93.792;

      // this is first callsign looked up by us, prepare the RingBuffer!
      if (callsign_lookup_history == NULL) {
         callsign_lookup_history_sz = cfg_get_int(cfg, "ui/callsign-lookup-history");
         if (callsign_lookup_history_sz <= 1) {
            callsign_lookup_history_sz = 1;
         }

         if ((callsign_lookup_history = rb_create(callsign_lookup_history_sz, "callsign lookup history")) == NULL) {
            fprintf(stderr, "render_call_lookup: out of memory!\n");
            exit(ENOMEM);
         }
      }

      // if we made it this far, add it to the history, since it's new...
      rb_add(callsign_lookup_history, fake_q, true);
   }
   render_call_lookup(fake_q);
   tb_present();
}

void redraw_screen(void) {
   tb_clear();
   if (height >= MIN_HEIGHT && width >= MIN_WIDTH) {
      // Show help (keys)
      print_help();
      // redraw all TextAreas
   //   ta_redraw_all();
      draw_fake_ta();
      // print the input prompt
      print_input();
      // and the status line
      print_status();
   } else {
      printf_tb(1, 1, TB_RED|TB_BOLD, 0, "[display] Your terminal has a size of %dx%d, this is too small!", width, height);
      printf_tb(1, 2, TB_RED|TB_BOLD, 0, "Please resize it to at least %dx%d!\n", MIN_WIDTH, MIN_HEIGHT);
      tb_hide_cursor();
   }
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
}
