/*
 * Support for help dialogs.
 *
 * These will use help files under etc/help/ *.hlp to display context-sensitive help to users
 *
 * This can be combined with tui-keymap to provide a more friendly user interface.
 */
#include "config.h"
#include "debuglog.h"
#include "tui.h"
#include "util.h"

#define	MAX_HELP_LINES	400		// maximum lines in a help file to load to memory

typedef struct help_data {
   FILE *fp;				// file pointer to the .hlp file we opened or NULL
   int height, width;			// height and width in percent, rounded to the next whole line
   rb_buffer_t *data;			// the loaded data
} help_data_t;

/*
 * Help files consist of following format:
 *
 * [options]
 * height=100%
 * width=100%
 * allow-ext-escapes=true
 * name="Main Screen"
 *
 * [text]
 * Welcome to ft8goblin! Here you can find some help for using the program!
 */
help_data_t *help_load(const char *name) {
   FILE *fp = NULL;
   char *path = NULL;
   help_data_t *hd = NULL;
   int	line = 0, errors = 0, warnings = 0;
   char buf[768];		/* line buffer for reads -- this should be quite excessive */
   char *end = NULL, *skip = NULL, *key = NULL, *val = NULL, *this_section = NULL;

   if (name == NULL) {
      log_send(mainlog, LOG_DEBUG, "help_load called with NULL name");
      return NULL;
   }

   if ((path = malloc(PATH_MAX + 1)) == NULL) {
      fprintf(stderr, "help_load: path: out of memory!\n");
      exit(ENOMEM);
   }

   if ((hd = malloc(sizeof(help_data_t))) == NULL) {
      fprintf(stderr, "help_load: help_data_t: out of memory!\n");
      exit(ENOMEM);
   }
   memset(hd, 0, sizeof(help_data_t));

   hd->data = rb_create(MAX_HELP_LINES, name);
   snprintf(path, PATH_MAX + 1, "etc/help/%s.hlp", name);
   if (path != NULL && is_file(path)) {
      //
      log_send(mainlog, LOG_INFO, "Opening help file %s!", path);
      if ((fp = fopen(path, "r")) == NULL) {
         log_send(mainlog, LOG_WARNING, "Failure opening help file %s: %d: %s", path, errno, strerror(errno));

         if (hd != NULL) {
            free(hd);
            hd = NULL;
         }
      }
   } else {
      log_send(mainlog, LOG_WARNING, "Failure opening help file %s: file %s", path, (is_file(path) ? "exists" : "doesn't exist"));

      if (hd != NULL) {
         free(hd);
         hd = NULL;
      }
   }
   free(path);
   path = NULL;

   fseek(fp, 0, SEEK_SET);
   do {
      memset(buf, 0, sizeof(buf));
      fgets(buf, sizeof(buf) - 1, fp);
      line++;

      skip = buf;
      while (*skip == ' ' || *skip == '\t') {
         skip++;
      }

      end = buf + strlen(buf);

      do {
         *end = '\0';
         end--;
      } while (*end == '\r' || *end == '\n' || *end == '\t' || *end == ' ');

      // we ate the whole line?
      if ((end - skip) <= 0) {
         continue;
      }

      if ((*skip == '/' && *skip + 1 == '/') ||
          (*skip == '#' || *skip == ';')) {
         continue;
      } else if (*skip == '[' && *end == ']') {
         if (this_section != NULL) {
            free(this_section);
         }
         this_section = strndup(skip + 1, strlen(skip) - 2);
         continue;
      } else if (*skip == '@') {
         // XXX: preprocessor directives? if/else/endif/include
      }

      if (!this_section) {
         log_send(mainlog, LOG_DEBUG, "help file %s contains lines outside section at %d: %s", name, line, buf);
         errors++;
         continue;
      }

      log_send(mainlog, LOG_DEBUG, "cfg load: section: %s: line: %d read: %s", this_section, line, buf);

      if (strncasecmp(this_section, "options", 7) == 0) {
         key = strtok(skip, "= \n");
         val = strtok(NULL, "= \n");
         log_send(mainlog, LOG_DEBUG, "options: %s = %s", key, val);
      } else if (strncasecmp(this_section, "text", 4) == 0) {
         size_t textlen = (end - skip);
         char *tp = NULL;

         if (textlen <= 0) {
            log_send(mainlog, LOG_DEBUG, "cfg load: options: textlen: %d is too small", textlen);
            goto fail;
         }

         if ((tp = strndup(buf, textlen + 1)) == NULL) {
            fprintf(stderr, "cfg load: text: out of memory!\n");
            goto fail;
         }
         
         // add to buffer, with needs_freed flag set
         rb_add(hd->data, (void *)tp, true);
      }
   } while(!feof(fp));
   free(this_section);
   return hd;

fail:
   if (hd != NULL) {
      free(hd);
      hd = NULL;
   }
   return NULL;
}

bool help_show(const char *name) {
   help_data_t *hd = help_load(name);
   if (hd == NULL) {
      return false;
   } else {
      return true;
   }
   return false;
}
