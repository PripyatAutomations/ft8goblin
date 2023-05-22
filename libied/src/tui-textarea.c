/*
 * tui TextArea widget
 *
 * This code probably isn't very reusable yet. That's being worked on...
 */
#include <libied/cfg.h>
#include <libied/debuglog.h>
#include <libied/tui.h>
#include <libied/subproc.h>
#include <errno.h>
#include <termbox2/termbox2.h>
extern TextArea *msgbox;

// Let's not make this public, but rather provide any utilities needed...
static TextArea *ta_textareas[MAX_TEXTAREAS];

//////////////////////////////////////////////
// Redraw the TextArea from the ring buffer //
//////////////////////////////////////////////
void ta_redraw(TextArea *ta) {
   if (ta->scrollback == NULL) {
      log_send(mainlog, LOG_CRIT, "ta_redraw: ta (%p) -> scrollback == NULL", ta);
      return;
   }
#if	1
   ////////////////////////////////////
   // Find the end of the ringbuffer //
   ////////////////////////////////////
   rb_node_t *rbn = ta->scrollback->head;
   rb_node_t *latest_rn = rbn;
   int loops = 0;
   size_t max_nodes = ta->scrollback->current_size;
   do {
      // A safety mechanism since i dislike while (true)...
      loops++;
      if (loops > max_nodes + 1)
         break;

      if (rbn == NULL) {
         break;
      }

      if (rbn->timestamp.tv_sec > latest_rn->timestamp.tv_sec) {
         latest_rn = rbn;
      }

      if (rbn->next == NULL) {
         break;
      }
      if (rbn->next != NULL) {
         rbn = rbn->next;
      } else {
         break;
      }
   } while (true);

   // XXX: rb_get_most_recent may be buggy, let's check...
   void *sb = rb_get_most_recent(ta->scrollback);
   if (sb != latest_rn) {
      log_send(mainlog, LOG_DEBUG, "ta_redraw: rb_get_most_recent() <%p> doesn't agree with local calculated <%p> end of ring buffer, this is probably a bug!", sb, latest_rn);
   } else {
      log_send(mainlog, LOG_DEBUG, "ta_redraw: rb_get_most_recent() agrees with local calculated end of ring buffer. It's probably safe of production!");
   }

   // latest_rn contains out end of the list or NULL
   if (latest_rn == NULL) {
      log_send(mainlog, LOG_DEBUG, "ta_redraw: latest_rn == (NULL)");
      return; 
   }

   //////////////////////////////////////////
   // Draw the TextArea from the bottom up //
   //////////////////////////////////////////
   for (int i = ta->bottom; i > ta->top; i--) {
      // Draw the current line of the ring buffer
      int offset = 0;
      char *bp = (char *)latest_rn->data;
      if (bp == NULL) {
         log_send(mainlog, LOG_CRIT, "ta_redraw: ta: %p sb: %p latest_rn: %p ->data: (NULL)", ta, ta->scrollback, latest_rn);
         continue;	// XXX: is this the right action here?
      }

      // Is the previous line valid?
      // If not, break
//      break;
      // If so, render it
      printf_tb(offset, i, TB_DEFAULT, TB_DEFAULT, "%s", latest_rn->data);
   }
   tb_present();
#endif
}

void ta_resize(TextArea *ta) {
   // XXX: We have to calculate sizes, if this is to be used for multiple TextAreas....
}

//////////////////////////////////////////
// Append to the end of the ring buffer //
//////////////////////////////////////////
int ta_append(TextArea *ta, const char *buf) {
   int rv = 0;

   if (ta == NULL || buf == NULL) {
      return -1;
   }

   // duplicate the buffer
   char *bp = strdup(buf);

   // set needs_freed to ensure it gets freed automatically...
   rb_add(ta->scrollback, bp, true);

   // XXX: temporarily send TextArea messages to log until TextArea finished
   log_send(mainlog, LOG_DEBUG, "<TextArea:%s> %s", ta->name, buf);
   return rv;
}

///////////////////////////
// Print to the TextArea //
///////////////////////////
void ta_printf(TextArea *ta, const char *fmt, ...) {
    if (ta == NULL || fmt == NULL)
       return;

    char buf[4096];
    int bg = 0, fg = 0;
    va_list vl;
    va_start(vl, fmt);

    vsnprintf(buf, sizeof(buf), fmt, vl);
    va_end(vl);
    ta_append(ta, buf);
}

void ta_destroy(TextArea *ta) {
   if (ta == NULL) {
      return;
   }

   if (ta->scrollback != NULL) {
      rb_destroy(ta->scrollback);
      ta->scrollback = NULL;
   }

   for (int i = 0; i < MAX_TEXTAREAS; i++) {
      if (ta_textareas[i] == ta) {
         ta_textareas[i] = NULL;
         i++;
         // Work backwards from the end, bumping all the entries down one..
         for (int j = MAX_TEXTAREAS; j < i; j--) {
            log_send(mainlog, LOG_DEBUG, "Moving ta_textareas slot %d to slot %i", j, j - 1);
            ta_textareas[j - 1] = ta_textareas[j];
         }
         break;
      }
   }
   free(ta);
}

TextArea *ta_init(const char *name, int scrollback_lines) {
   TextArea *ta = NULL;

   if ((ta = malloc(sizeof(TextArea))) == NULL) {
      fprintf(stderr, "ta_init: out of memory!\n");
      exit(ENOMEM);
   }
   memset(ta, 0, sizeof(TextArea));

   // save the name
   strncpy(ta->name, name, 64);

   /////////////////////////////////
   // Setup the scrollback buffer //
   /////////////////////////////////
   ta->scrollback_lines = scrollback_lines;

   if (ta->scrollback_lines == -1) {
      log_send(mainlog, LOG_CRIT, "ta_init: refusing to create TextArea with no scrollback as this can't hold text!");
      return NULL;
   }
   ta->scrollback = rb_create(ta->scrollback_lines, name);

   // add to end of ta_textareas array
   for (int i = 0; i < MAX_TEXTAREAS; i++) {
      if (ta_textareas[i] == NULL) {
         ta_textareas[i] = ta;
         break;
      }
   }
   return ta;
}

// redraw *ALL* textareas
void ta_redraw_all(void) {
   for (int i = 0; i < MAX_TEXTAREAS; i++) {
      if (ta_textareas[i] == NULL)
         continue;
      ta_redraw(ta_textareas[i]);
   }
}

// send resize all textareas
void ta_resize_all(void) {
   for (int i = 0; i < MAX_TEXTAREAS; i++) {
      if (ta_textareas[i] == NULL)
         continue;
      ta_resize(ta_textareas[i]);
   }
}

/*
//      struct tb_event *evt;
//      void (*callback)();
Keymap *main_keymap[] = {
   {
      .evt = {
         .type == TB_EVENT_KEY,
         .key = TB_KEY_ESC
      },
   },
   {
      .evt = NULL
   }
}
*/