#include "config.h"
#include "debuglog.h"
#include "util.h"
#include "ft8goblin_types.h"

// toggle an intbool (<= 0 is off, >= 1 is on)
void toggle(int *v) {
   if (*v <= 0)
      *v = 1;
   else if (*v >= 1)
      *v = 0;
}

int is_file(const char *path) {
   struct stat sb;

   if (stat(path, &sb) != 0)
      return 0;

   if (S_ISREG(sb.st_mode)) {
//      log_send(mainlog, LOG_DEBUG, "is_file: %s: true", path);
      return 1;
   }

   return 0;
}

int is_dir(const char *path) {
   struct stat sb;

   if (stat(path, &sb) != 0)
      return 0;

   if (S_ISDIR(sb.st_mode))
      return 1;

   return 0;
}

int is_link(const char *path) {
   struct stat sb;

   if (stat(path, &sb) != 0)
      return 0;

   if (S_ISLNK(sb.st_mode))
      return 1;

   return 0;
}

int is_fifo(const char *path) {
   struct stat sb;

   if (stat(path, &sb) != 0)
      return 0;

   if (S_ISFIFO(sb.st_mode))
      return 1;

   return 0;
}
