/*
 * Our overly complicated text based user interface based on termbox2!
 *
 * An effort is being made to make this reusable for other programs...
 *
 * Widgets Available:
 *	TextArea 	Scrollable text area, wrapped around termbox2 (with autoscroll option)
 *	Menu		Multi-level menus, with history support, similar to WANG mainframes.
 *	InputArea	Single line input box with cursor controls and history.
 *
 */
#include <libied/cfg.h>
#include <libied/debuglog.h>
#include <libied/tui.h>
#include <libied/subproc.h>
#include <errno.h>
#include <regex.h>
#include <termbox2/termbox2.h>

TextArea *msgbox = NULL;
bool dying = false;
time_t now = 0;
int	line_status = -1;		// status line
int 	line_input = -1;		// input field
int	height = -1, width = -1;

void	(*redraw_screen_cb)(void) = NULL;

////////////////////////////////////////////////////////////
// These let us print unicode to an exact screen location //
// - We call these after clearing screen when scrolling.  //
////////////////////////////////////////////////////////////
void print_tb(const char *str, int x, int y, uint16_t fg, uint16_t bg) {
   while (*str) {
       uint32_t uni;
       str += tb_utf8_char_to_unicode(&uni, str);
       tb_set_cell(x, y, uni, fg, bg);
       x++;
   }
}

void printf_tb(int x, int y, uint16_t fg, uint16_t bg, const char *fmt, ...) {
   char buf[8192];
   va_list vl;
   va_start(vl, fmt);
   vsnprintf(buf, sizeof(buf), fmt, vl);
   va_end(vl);
   print_tb(buf, x, y, fg, bg);
}

//////////////////////////
// Parse out color tags //
//////////////////////////
// XXX: Really we need shorter tokens for these like
int parse_color(const char *str) {
   int rv = -1;

   if (strncmp(str, "BLACK", 5) == 0) {
      rv = TB_BLACK|TB_BOLD;
   } else if (strncmp(str, "black", 5) == 0) {
      rv = TB_BLACK;
   } else if (strncmp(str, "RED", 3) == 0) {
      rv = TB_RED|TB_BOLD;
   } else if (strncmp(str, "red", 3) == 0) {
      rv = TB_RED;
   } else if (strncmp(str, "GREEN", 5) == 0) {
      rv = TB_GREEN|TB_BOLD;
   } else if (strncmp(str, "green", 5) == 0) {
      rv = TB_GREEN;
   } else if (strncmp(str, "YELLOW", 6) == 0) {
      rv = TB_YELLOW|TB_BOLD;
   } else if (strncmp(str, "yellow", 6) == 0) {
      rv = TB_YELLOW;
   } else if (strncmp(str, "BLUE", 4) == 0) {
      rv = TB_BLUE|TB_BOLD;
   } else if (strncmp(str, "blue", 4) == 0) {
      rv = TB_BLUE;
   } else if (strncmp(str, "MAGENTA", 7) == 0) {
      rv = TB_MAGENTA|TB_BOLD;
   } else if (strncmp(str, "magenta", 7) == 0) {
      rv = TB_MAGENTA;
   } else if (strncmp(str, "CYAN", 4) == 0) {
      rv = TB_CYAN|TB_BOLD;
   } else if (strncmp(str, "cyan", 4) == 0) {
      rv = TB_CYAN;
   } else if (strncmp(str, "WHITE", 5) == 0) {
      rv = TB_WHITE|TB_BOLD;
   } else if (strncmp(str, "white", 5) == 0) {
      rv = TB_WHITE;
   } else
      rv = TB_DEFAULT;

   return rv;
}

// Use parse_color above to parse out color strings, whether just $FG$ or $FG,BG$
ColorPair parse_color_str(const char *str) {
   ColorPair cp = { 0, 0};
   

   // XXX: Find $...$ pattern

   return cp;
}

void tui_redraw(void) {
   if (redraw_screen_cb != NULL) {
      redraw_screen_cb();
   }
}

void tui_resize_window(struct tb_event *evt) {
   // if we got passed an event, use it's data, otherwise query termbox2 for height/width of screen
   if (evt == NULL) {
      height = tb_height();
      width = tb_width();
   } else {
      height = evt->h;
      width = evt->w;
   }

   if (width < 90 || height < 25) {
      tb_clear();
      tb_present();
      log_send(mainlog, LOG_CRIT, "Your terminal has a size of %dx%d, this is too small! I cannot continue...", width, height);
      ta_printf(msgbox, "$RED$Your terminal has a size of %dx%d, this is too small! I cannot continue...\n", width, height);
      tb_present();
      return;
   } else {
      log_send(mainlog, LOG_NOTICE, "display resolution %dx%d is acceptable!", width, height);
      ta_resize_all();
      tui_redraw();
   }
   line_status = height - 1;
   line_input = height - 2;
}

void tui_shutdown(void) {
   // Tear down to exit
   dying = true;

   // display a notice that we are exiting and to be patient...
   tb_clear();
//   modal_dialog(0, TB_WHITE|TB_BOLD, TB_RED, TB_BLACK, TB_RED|BOLD, "SHUTTING DOWN", TB_RED|TB_BOLD, TB_BLACK, "ft8goblin is shutting down, please wait...");

   // stop libev stuff...
   // libev_shutdown();

   // send SIGTERM then SIGKILL to subprocesses...
   subproc_shutdown_all();

   // shut down termbox
   tb_shutdown();
}

void tui_init(void (*cb)()) {
   redraw_screen_cb = cb;
   tb_init();
   tb_set_input_mode(TB_INPUT_ALT | TB_INPUT_MOUSE);
   // we should look at this again when we get configurable color schemes going
   int color_mode = cfg_get_int(cfg, "ui/color-mode");
   // 256 or 216?
   if (color_mode == 216) {
      tb_set_output_mode(TB_OUTPUT_216);
   } else if (color_mode == 256) {
      tb_set_output_mode(TB_OUTPUT_256);
   }
}

//////////
// ToDo //
//////////

// Returns: index of button pressed. (Buttons are 0 = OK, 1 = CANCEL, 2 = OK|CANCEL, 3 = OK|CANCEL|HELP)
int modal_dialog(int buttons, int border_fg, int border_bg, int title_fg, int title_bg, const char *title, int text_fg, int text_bg, const char *fmt, ...) {
   va_list ap;

   if (fmt != NULL) {
      va_start(ap, fmt);

      // print the buffer into temporary memory
      char buf[2000];		// an 80x25 screen is 2000 characters..
      memset(buf, 0, 2000);
      vsnprintf(buf, 2000, fmt, ap);
      // we should check return value and errno... ;)

      // XXX: Display into the the modal window
      va_end(ap);
   } else {
      // handle setting up a dialog for "other" uses (not yet supported)
      log_send(mainlog, LOG_CRIT, "modal_dialog doesn't yet support use other than simple alerts. (fmt is NULL). Please fix your code or improve the library!");
      return ENOSYS;
   }
   return 0;
}
