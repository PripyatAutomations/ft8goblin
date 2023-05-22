#include <libied/cfg.h>
#include <libied/debuglog.h>
#include <libied/util.h>
#include <limits.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
// XXX: We should mirror log messages above a set priority into the msgbox TextArea if it exists ;)
// from main program usually
extern char *progname;
extern bool dying;
LogHndl *mainlog = NULL;

static struct log_levels {
   const char *str;
   int level;
} log_levels[] = {
   { "debug", LOG_DEBUG },
   { "info", LOG_INFO },
   { "notice", LOG_NOTICE },
   { "warning", LOG_WARNING },
   { "error", LOG_ERR },
   { "critical", LOG_CRIT },
   { "alert", LOG_ALERT },
   { "emergency", LOG_EMERG },
   { NULL, -1 },
};

static inline const char *LogName(int level) {
   struct log_levels *lp = log_levels;

   do {
      if (lp->level == level)
         return lp->str;

      lp++;
   } while(lp->str != NULL);

   return LOG_INVALID_STR;
}

static inline const int LogLevel(const char *name) {
   struct log_levels *lp = log_levels;

   do {
      if (strcasecmp(lp->str, name) == 0)
         return lp->level;

      lp++;
   } while(lp->str != NULL);

   return -1;
}

int log_level = -1;

int log_send_va(LogHndl *log, int level, const char *msg, va_list ap) {
   char datebuf[32];
   char buf[4096];
   int max;
   time_t now = time(NULL);
   FILE *fp = stderr;		// default to stderr til logging is up

   // create data stamp
   memset(datebuf, 0, sizeof(datebuf));
   strftime(datebuf, sizeof(datebuf) - 1, "%Y-%m-%d %H:%M:%S", localtime(&now));

   if (log_level < level)
      return -1;

   if (log) {
      if (log->type == LOG_syslog) {
         vsyslog(level, msg, ap);
      } else if (log->type != NONE) {
         memset(buf, 0, 4096);
         vsnprintf(buf, 4095, msg, ap);
      }

      if (log->type != LOG_stderr && log->fp)
         fp = log->fp;
   }

   // if we're dying, try to write to stderr too...
   if (dying) {
      fprintf(stderr, "%s [%s] %s\n", datebuf, LogName(level), buf);
      fflush(stderr);
   }

   fprintf(fp, "%s [%s] %s\n", datebuf, LogName(level), buf);

   // force it to disk
   fflush(fp);

   va_end(ap);
   return 0;
}

int log_send(LogHndl *log, int level, const char *msg, ...) {
   va_list ap;
   va_start(ap, msg);
   return log_send_va(log, level, msg, ap);
}

LogHndl *log_open(const char *path) {
   LogHndl *log = NULL;

   if (path == NULL) {
      fprintf(stderr, "log_open: attempting to open NULL logfile!!\n");
      exit(1);
//      return NULL;
   }

   if ((log = (LogHndl *)malloc(sizeof(LogHndl))) == NULL) {
      fprintf(stderr, "log_init: out of memory!\n");
      exit(ENOMEM);
   }

   // if log level hasn't been set yet, set it
   if (log_level == -1) {
      char buf[PATH_MAX + 1];
      memset(buf, 0, PATH_MAX + 1);
      snprintf(buf, PATH_MAX, "logging/%s-loglevel", progname);
      const char *p = cfg_get_str(cfg, buf);
      if (p != NULL) {
         int x = LogLevel(p);

         if (x > 0) {
            log_level = x;
         } else {
            log_level = LOG_NOTICE;
         }
      } else {
         log_level = LOG_DEBUG;
      }
//      fprintf(stderr, "opening log at %s with log_level %s\n", path, p);
   }

   if (strcasecmp(path, "syslog") == 0) {
      log->type = LOG_syslog;
      openlog(progname, LOG_NDELAY|LOG_PID, LOG_DAEMON);
   } else if (strcasecmp(path, "stderr") == 0) {
      log->type = LOG_stderr;
      log->fp = stderr;
   } else if (strncasecmp(path, "fifo://", 7) == 0) {
      if (is_fifo(path + 7) || is_file(path + 7))
         unlink(path + 7);

      mkfifo(path+7, 0600);

      if (!(log->fp = fopen(path + 7, "w"))) {
         fprintf(stderr, "Failed opening log fifo '%s' %d:%s\n", path+7, errno, strerror(errno));
         log->fp = stderr;
      } else
         log->type = LOG_fifo;
   } else if (strncasecmp(path, "file://", 7) == 0) {
      if (!(log->fp = fopen(path + 7, "a+"))) {
         fprintf(stderr, "failed opening log file '%s' %d:%s\n", path+7, errno, strerror(errno));
         log->fp = stderr;
      } else
         log->type = LOG_file;
   }

   return log;
}

void log_close(LogHndl *log) {
   if (log == NULL)
      return;

   if (log->type == LOG_file || log->type == LOG_fifo) {
      fflush(log->fp);
      fclose(log->fp);
   } else if (log->type == LOG_syslog) {
      closelog();
   }

   free(log);
   log = NULL;
}
