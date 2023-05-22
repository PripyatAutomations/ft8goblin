#if	!defined(_syslog_h)
#define	_syslog_h
#include <syslog.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>
/////////////////////////////////////////////
// private bits used only internally
#define LOG_INVALID_STR "*invalid_level*"

#ifdef __cplusplus
extern "C" {
#endif
   enum log_target_type { NONE = 0, LOG_syslog, LOG_stderr, LOG_file, LOG_fifo };
   typedef struct {
     // private (do not modify)
     enum log_target_type type;
     FILE *fp;
   } LogHndl;
   ///////////////////////////////////////////

   //////////////////////
   // Public interface //
   //////////////////////
   int log_send(LogHndl *log, int level, const char *msg, ...);
   int log_send_va(LogHndl *log, int level, const char *msg, va_list);
   extern LogHndl *log_open(const char *path);
   extern void log_close(LogHndl *log);
   extern LogHndl *mainlog;
#ifdef __cplusplus
};
#endif

#endif	// !defined(_syslog_h)
