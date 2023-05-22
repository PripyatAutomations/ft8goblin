/*
 * This needs much improvement and a lot of src/tui-textarea.c belongs as a keymap here
 */
#include <libied/cfg.h>
#include "config.h"
#include "ft8goblin_types.h"
#include <libied/subproc.h>
#include <libied/util.h>
#include <libied/tui.h>
#include <libied/debuglog.h>
#include <libied/daemon.h>
#include <libied/subproc.h>
#include "watch.h"
#include "qrz-xml.h"
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
#define	MIN_WIDTH	110

// ugh globals...
char *progname = "ft8goblin";
const char *mycall = NULL;	// cfg:ui/mycall
const char *gridsquare = NULL;	// cfg:ui/gridsquare
bool	tx_enabled = false;		// Master toggle to TX mode.
bool	tx_pending = false;
int	active_band = 40;		// Which band are we TXing on?
bool	tx_even = false;		// TX even or odd cycle?
bool	cq_only = false;		// Only show CQ & active QSOs?
int	active_pane = 0;		// active pane (0: TextArea, 1: TX input)
bool	auto_cycle = true;		// automatically switch to next message on RXing a response
tx_mode_t tx_mode = TX_MODE_NONE;	// mode
rb_buffer_t *callsign_lookup_history = NULL;
size_t callsign_lookup_history_sz = 1;
int	max_rigs = 0;			// maximum rigs

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

   printf_tb(offset, 0, TB_RED|TB_BOLD, 0, "^P ");
   offset += 3;
   printf_tb(offset, 0, TB_MAGENTA|TB_BOLD, 0, "Even/Odd");
   offset = 7;

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "F1 ");
   offset += 3;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "Help ");
   offset += 5;

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

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "^Y ");
   offset += 3;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "Auto ");
   offset += 5;

   printf_tb(offset, 1, TB_RED|TB_BOLD, 0, "^O ");
   offset += 3;
   printf_tb(offset, 1, TB_MAGENTA|TB_BOLD, 0, "TXMode ");
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

   // TX mode
   const char *str = get_mode_name(tx_mode);

   if (str != NULL) {
      printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "[");
      offset++;
      printf_tb(offset, line_status, TB_CYAN, 0, "TX Mode:");
      offset += 8;
      printf_tb(offset, line_status, TB_YELLOW|TB_BOLD, 0, "%s", str);
      offset += strlen(str);
      printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "] ");
      offset += 2;
   }

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
   // Explode the list of radios actively PTTing
#if	0
   int active_rigs = 0;
   for (int i = 0; i < max_rigs; i++) {
      if (rigs[i].ptt_active) {
         active_rigs++;
         // only show once
         if (active_rigs == 1) {
            printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "[");
            offset++;
            printf_tb(offset, line_status, TB_GREEN|TB_BOLD, 0, "PTT:");
            offset += 4;
         }
         printf_tb(offset, line_status, TB_RED|TB_BOLD, 0, "%d", i);
      }
   }
   if (active_rigs > 0) {
      printf_tb(offset, line_status, TB_WHITE|TB_BOLD, 0, "] ");
      offset += 2;
   }
#endif

   // version
   char verbuf[128];
   memset(verbuf, 0, 128);
   snprintf(verbuf, 128, "ft8goblin/%s", VERSION);
   size_t ver_len = strlen(verbuf);
   printf_tb(tb_width() - ver_len, line_status, TB_MAGENTA|TB_BOLD, 0, "%s", verbuf);
   tb_present();
}

// XXX: Need to make this use of TextArea functions
// Renders the data passed in a calldata_t to the callsign lookup 'window'
void render_call_lookup(calldata_t *calldata) {
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
   printf_tb(x, y++, TB_CYAN|TB_BOLD, TB_BLACK, "DXCC %d Grid %s = %3.3f,%3.3f", calldata->dxcc, calldata->grid,
      calldata->latitude, calldata->longitude);

   printf_tb(x, y++, TB_WHITE|TB_BOLD, TB_BLACK, "Distance: 1102 mi, heading 293°");
   printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "Lic. Class: ");
   x += 12;
   if (calldata->opclass[0] == 'E') {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "Extra");
      x += 5;
   } else if (calldata->opclass[0] == 'G') {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "General");
      x += 7;
   } else if (calldata->opclass[0] == 'A') {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "Advanced");
      x += 9;
   } else if (calldata->opclass[0] == 'T') {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "Technician");
      x += 10;
   } else if (calldata->opclass[0] == 'N') {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, "Novice");
      x += 6;
   } else {
      log_send(mainlog, LOG_DEBUG, "render_call_lookup: can't grok operator class <%s> for %s", calldata->opclass, calldata->callsign);
   }

   if (strchr(calldata->codes, 'V') != NULL) {
      printf_tb(x, y, TB_WHITE|TB_BOLD, TB_BLACK, " (Vanity)");
   }
      
   y++; x = default_x + 1;;
   printf_tb(x, y++, TB_WHITE, TB_BLACK, "Prev. Calls: %s", calldata->previous_call);
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

   if (calldata->address_attn[0] != '\0') {
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
calldata_t *fake_q;

void draw_fake_ta(void) {
   char datebuf[128];

   time_t mynow = time(NULL);
   struct tm *tm = NULL;
   tm = localtime(&mynow);
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
      if ((fake_q = malloc(sizeof(calldata_t))) == NULL) {
         fprintf(stderr, "draw_fake_ta: out of memory!\n");
         exit(ENOMEM);
      }

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
//      ta_redraw_all();
//      draw_fake_ta();
      // print the input prompt
      tui_show_input();
      // and the status line
      print_status();
   } else {
      printf_tb(1, 1, TB_RED|TB_BOLD, 0, "[display] Your terminal has a size of %dx%d, this is too small!", width, height);
      printf_tb(1, 2, TB_RED|TB_BOLD, 0, "Please resize it to at least %dx%d!\n", MIN_WIDTH, MIN_HEIGHT);
      printf_tb(1, 3, TB_YELLOW|TB_BOLD, 0, "You can use ctrl-X or ctrl-q to exit or resize your terminal to continue.\n");
      tb_hide_cursor();
   }
   tb_present();
}

int main(int argc, char **argv) {
   const char *logpath = NULL;
   struct ev_loop *loop = EV_DEFAULT;

   // print this even though noone will see it, except in case of error exit ;)
   printf("ft8goblin: A console based ft8 client with support for multiband operation\n\n");

   // This can't work without a valid configuration...
   if (!(cfg = load_config()))
      exit_fix_config();

   // setup some values that are frequently requested
   mycall = cfg_get_str(cfg, "site/mycall");
   gridsquare = cfg_get_str(cfg, "site/gridsquare");

   if ((logpath = dict_get(runtime_cfg, "logpath", "file://ft8goblin.log")) != NULL) {
      mainlog = log_open(logpath);
   } else {
      log_open("stderr");
   }
   log_send(mainlog, LOG_NOTICE, "ft8goblin/%s starting up!", VERSION);


   // setup the TUI toolkit
   tui_init(&redraw_screen);
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
   // setup subprocess keeper
   subproc_init();
   //	sigcapd (one instance)

   //	ft8capture (single instance per device)
        // XXX: Walk the tree at cfg:devices
        // XXX: Walk the tree at cfg:bands
        // XXX: Figure out which bands can be connected to which devices and start slicers in sigcapd

   //	ft8decoder (one per band)

   // 	callsign-lookupd (one instance)
   const char *args[2] = { "./bin/callsign-lookup", NULL };
   int callsign_lookupd_slot = subproc_create("callsign-lookup:main", "./bin/callsign-lookup", args, 2);
   if (callsign_lookupd_slot < 0) {
      log_send(mainlog, LOG_CRIT, "Failed to start callsign-lookup process");
   }

   // main loop...
   while (!dying) {
      // XXX: do libev stuff and decide if there's waiting input on fd_tb_tty or fd_fb_resize and act appropriately...
      ev_run (loop, 0);

      // if ev loop exits, we need to die..
      dying = true;
   }

   subproc_shutdown_all();
   tui_shutdown();
   log_close(mainlog);
   mainlog = NULL;
   fini(0);			// remove pidfile, etc
   return 0;
}

/////////////////////////////////////////////
// return a static string of the mode name //
/////////////////////////////////////////////
const char *mode_names[] = {
   "OFF",
   "FT8",
   "FT4",
//   "JS8",
//   "PSK11",
//   "ARDOP FEC",
   NULL
};
const char *get_mode_name(tx_mode_t mode) {
   if (mode <= TX_MODE_NONE || mode >= TX_MODE_END) {
      return mode_names[0];
   }
   return mode_names[mode];
}

void toggle_tx_mode(void) {
   if (tx_mode == TX_MODE_NONE || tx_mode == TX_MODE_END) {
      tx_mode = TX_MODE_NONE + 1;
   } else if (tx_mode < TX_MODE_END) {
      tx_mode++;
   }

   log_send(mainlog, LOG_INFO, "Toggled TX mode to %s", get_mode_name(tx_mode));
   redraw_screen();
}

///////////
// Menus //
///////////
int view_config(void) {
   // XXX: Show the yajl config tree as a scrollable 'menu'
   return 0;
}

void halt_tx_now(void) {
   ta_printf(msgbox, "$RED$Halting TX!");
   tx_enabled = false;
   log_send(mainlog, LOG_CRIT, "Halting TX immediately at user request!");
   redraw_screen();
}
