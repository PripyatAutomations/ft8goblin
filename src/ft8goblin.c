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

// BUG: These belong in a header...
#define	MIN_HEIGHT	25
#define	MIN_WIDTH	90

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
bool	auto_cycle = true;		// automatically switch to next message on RXing a response

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

   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^W ");
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

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "F2-F12 ");
   offset += 7;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "Automsgs ");
   offset += 9;

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

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "^A ");
   offset += 3;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "Auto ");
   offset += 5;
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

   printf_tb(offset, line_status, TB_YELLOW|TB_BOLD, 0, "%s ", outstr);
   offset += 9;

   // callsign
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "[Oper:");
   offset += 6;
   printf_tb(offset, line_status, TB_CYAN|TB_BOLD, 0, "%s", mycall);
   offset += strlen(mycall);
   printf_tb(offset++, line_status, TB_RED|TB_BOLD, 0, "@");
   printf_tb(offset, line_status, TB_CYAN|TB_BOLD, 0, "%s", gridsquare);
   offset += strlen(gridsquare);
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;

   // only show CQ and active QSOs?
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "[");
   offset++;
   printf_tb(offset, line_status, TB_CYAN, 0, "CQonly:");
   offset += 7;

   if (cq_only) {
      printf_tb(offset, line_status, TB_RED|TB_BOLD, 0, "ON");
      offset += 2;
   } else {
      printf_tb(offset, line_status, TB_GREEN|TB_BOLD, 0, "OFF");
      offset += 3;
   }
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;

   // auto cycle
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "[");
   offset++;
   printf_tb(offset, line_status, TB_GREEN|TB_BOLD, 0, "Auto:");
   offset += 5;

   if (auto_cycle) {
      printf_tb(offset, line_status, TB_RED|TB_BOLD, 0, "ON");
      offset += 2;
   } else {
      printf_tb(offset, line_status, TB_GREEN|TB_BOLD, 0, "OFF");
      offset += 3;
   }
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;

   // TX enabled status
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "[");
   offset++;
   printf_tb(offset, line_status, TB_GREEN|TB_BOLD, 0, "TX:");
   offset += 3;

   if (tx_enabled) {
      printf_tb(offset, line_status, TB_RED|TB_BOLD, 0, "ON");
      offset += 2;
   } else {
      printf_tb(offset, line_status, TB_GREEN|TB_BOLD, 0, "OFF");
      offset += 3;
   }
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;


   // show bands with TX enabled, from yajl tree...
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "[");
   offset++;
   printf_tb(offset, line_status, TB_GREEN|TB_BOLD, 0, "TXBand:");
   offset += 7;

   if (active_band != 0) {
      printf_tb(offset, line_status, TB_RED|TB_BOLD, 0, "%dm", active_band);
      offset += 3;

      printf_tb(offset++, line_status, TB_WHITE|TB_BOLD, 0, "/");

      if (tx_even) {
         printf_tb(offset, line_status, TB_YELLOW|TB_BOLD, 0, "EVEN");
         offset += 4;
      } else {
         printf_tb(offset, line_status, TB_YELLOW|TB_BOLD, 0, "ODD");
         offset += 3;
      }
   }

   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;

   // print the PTT status
#if	0
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "[");
   offset++;
   printf_tb(offset, line_status, TB_GREEN|TB_BOLD, 0, "PTT:");
   offset += 4;
   // Explode the list of radios actively PTTing
   for (int i = 0; i < max_rigs; i++) {
      if (rigs[i].ptt_active) {
         printf_tb(offset, line_status, TB_RED|TB_BOLD, 0, "%d", i);
      }
   }
   printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "] ");
   offset += 2;
#endif

   char verbuf[128];
   memset(verbuf, 0, 128);
   snprintf(verbuf, 128, "ft8goblin/%s", VERSION);
   size_t ver_len = strlen(verbuf);
   printf_tb(tb_width() - ver_len, line_status, TB_MAGENTA|TB_BOLD, 0, "%s", verbuf);
   tb_present();
}

static void print_input(void) {
   if (active_pane == PANE_INPUT) {
      printf_tb(0, line_input, TB_GREEN|TB_BOLD, TB_BLACK, "> %s", input_buf);
      tb_set_cursor(2 + input_buf_cursor, line_input);
   } else {
      printf_tb(0, line_input, TB_WHITE, TB_BLACK, "> %s", input_buf);
      tb_hide_cursor();
   }
   tb_present();
}

// Renders the data passed in a qrz_callsign_t to a dialog window
void render_call_lookup(qrz_callsign_t *calldata) {
   int default_x = (width / 2) + 1,
       x = default_x,
       y = 2;

   if (calldata == NULL) {
      // error handling!
      log_send(mainlog, LOG_WARNING, "render_call_lookup passed NULL calldata");
      return;
   }

   // print callsign data
   int WIN_BORDER_FG = -1;
   int WIN_BORDER_BG = -1;
   size_t padlen = (width / 2) - 3;
   char padbuf[padlen + 1];
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
   printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "Lic. Class: ");
   x += 12;
   if (calldata->opclass[0] == 'E') {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "Extra");
   } else if (calldata->opclass[0] == 'G') {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "General");
   } else if (calldata->opclass[0] == 'A') {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "Advanced");
   } else if (calldata->opclass[0] == 'T') {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "Technician");
   } else if (calldata->opclass[0] == 'N') {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "Novice");
   } else {
      log_send(mainlog, LOG_DEBUG, "render_call_lookup: can't grok operator class <%s> for %s", calldata->opclass, calldata->callsign);
   }

   if (strchr(calldata->codes, 'V') != NULL) {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, " (Vanity)");
   }
      
   y++; x = default_x + 1;;
   printf_tb(x, y++, TB_RED|TB_BOLD, TB_BLACK, "Prev. Calls: %s", calldata->previous_call);
   // XXX: Add days til expires?


   char datebuf_eff[128], datebuf_exp[128];
   struct tm *tm;
   memset(datebuf_eff, 0, 128);
   memset(datebuf_exp, 0, 128);
   if ((tm = localtime(&calldata->license_effective)) == NULL) {
      log_send(mainlog, LOG_CRIT, "localtime() failed");
      exit(255);
   }

   if (strftime(datebuf_eff, 128, "%Y/%m/%d", tm) == 0 && errno != 0) {
      log_send(mainlog, LOG_CRIT, "strftime() failed");
      exit(254);
   }

   if ((tm = localtime(&calldata->license_expiry)) == NULL) {
      log_send(mainlog, LOG_CRIT, "localtime() failed");
      exit(255);
   }

   if (strftime(datebuf_exp, 128, "%Y/%m/%d", tm) == 0 && errno != 0) {
      log_send(mainlog, LOG_CRIT, "strftime() failed");
      exit(254);
   }
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Effective: %s", datebuf_eff);
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Expires: %s", datebuf_exp);
   printf_tb(x, y++, TB_YELLOW|TB_BOLD, TB_BLACK, "Logbook QSOs: 3 (2 bands)");
   // skip a line
   y++;
   printf_tb(x, y++, TB_GREEN|TB_BOLD, TB_BLACK, "%s %s", calldata->first_name, calldata->last_name);
   if (calldata->address_attn > 0) {
      printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "ATTN: %s", calldata->address_attn);
   }
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "%s", calldata->address1);
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "%s, %s %s %s", calldata->address2, calldata->state, calldata->zip, calldata->country);
   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Email: %s", calldata->email);
//   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Phone: %s", calldata->phone);
   if (calldata->url[0] > 0) {
      printf_tb(x, y++, TB_WHITE, TB_BLACK, "URL: %s", calldata->url);
   }
   if (calldata->origin == DATASRC_QRZ) {
      printf_tb(x, y++, TB_MAGENTA, TB_BLACK, "Data sourced from QRZ (11 hours old)");
   }

   x -= 1;
   printf_tb(x, height - 3, WIN_BORDER_FG, WIN_BORDER_BG, "└%s┘", padbuf);
}

char *pad_db(char *buf, size_t buf_len, int db, int digits) {
   int mydigits = 0;
   bool negative = false;

   if (buf == NULL || buf_len <= 0) {
      log_send(mainlog, LOG_DEBUG, "pad_db: buf <%p> or buf_len <%lu> invalid!", buf, buf_len);
      return NULL;
   }

   memset(buf, 0, buf_len);

   if (digits > 0) {
      // real world, +100dBm is 10 megawatts :o
      if (db > 99) {			// 100+
         mydigits = 3;
      } else if (db > 9) {		// 10-99
         mydigits = 2;
      } else if (db <= 9 && db >= 0) {	// 0-9
         mydigits = 1;
      } else if (db < -99) {		// -100+
         mydigits = 3;
      } else if (db < -9) {		// -10 - -99
         mydigits = 2;
      } else {				// -1 - -9
         mydigits = 1;
      }

      if (db < 0) {
         negative = true;
      }

      // truncate to digits max length
      if (mydigits > digits) {
         mydigits = digits;
      }

      size_t padlen = mydigits + 1 /* sign */ + 1 /* NULL */;
      char pad[padlen];
      memset(pad, 0, padlen);
      int mypad = (digits - mydigits) - 1;
      log_send(mainlog, LOG_DEBUG, "mypad: %d, padlen: %lu, mydigits: %d, digits: %d", mypad, padlen, mydigits, digits);
      if (negative) {
         //
      }
   } else {
      return NULL;
   }
   return buf;
}

// XXX: temporary thing for our mockup...
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
   size_t padlen = (width / 2) - 3;
   char padbuf[padlen + 1];
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
   printf_tb(x, y++, TB_CYAN|TB_BOLD, TB_RED, "***");

   // new line
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
      snprintf(fake_q->first_name, sizeof(fake_q->first_name), "Robert");
      snprintf(fake_q->last_name, sizeof(fake_q->last_name), "Test");
      snprintf(fake_q->address1, sizeof(fake_q->address1), "123 Sesame St");
      snprintf(fake_q->address2, sizeof(fake_q->address2), "Earth City");
      snprintf(fake_q->state, sizeof(fake_q->state), "MO");
      snprintf(fake_q->zip, sizeof(fake_q->zip), "63045");
      snprintf(fake_q->country, sizeof(fake_q->country), "USA");
      snprintf(fake_q->email, sizeof(fake_q->email), "bob@test.com");
      snprintf(fake_q->opclass, sizeof(fake_q->opclass), "G");
      snprintf(fake_q->codes, sizeof(fake_q->opclass), "HVIE");
//      snprintf(fake_q->phone, sizeof(fake_q->phone), "987-654-3210");
//      fake_q->country_code = 
      fake_q->latitude = 32.021;
      fake_q->longitude = -93.792;
      fake_q->license_expiry = 1835585999;
      fake_q->license_effective = 1519880400;
      fake_q->origin = DATASRC_QRZ;
      snprintf(fake_q->url, sizeof(fake_q->url), "https://qrz.com/db/AA1AB");

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

   // setup the TUI toolkit
   tui_init();
   tui_input_init();
   tui_io_watcher_init();

   // create the default TextArea for messages
   msgbox = ta_init("msgbox", cfg_get_int(cfg, "ui/scrollback-lines"));
   tui_resize_window(NULL);
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
