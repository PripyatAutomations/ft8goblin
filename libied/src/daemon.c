#define	_daemon_cpp
#include "config.h"
#include "debuglog.h"
#include "daemon.h"
#include "dict.h"
#include "ft8goblin_types.h"
#include <signal.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

int pidfd = -1;
const char *pidfile = NULL;
extern char *progname;

// Detach from the console and go into background
int daemonize(void) {
   struct stat sb;

   pidfile = dict_get(runtime_cfg, "pidfile", NULL);

   if (pidfile == NULL) {
      log_send(mainlog, LOG_CRIT, "no pidfile specified in runtime:pidfile\n");
      exit(255);
   }

   if (stat(pidfile, &sb) == 0) {
      log_send(mainlog, LOG_CRIT, "pidfile %s already exists, bailing!\n", pidfile);
      exit(1);
   }

   // are we configured to daemonize?
   if (dict_get_bool(runtime_cfg, "daemonize", false)) {
      printf("going to the background...\n");
      pid_t pid = fork();

      if (pid < 0) {
         log_send(mainlog, LOG_CRIT, "daemonize: Unable to fork(): %d (%s)", errno, strerror(errno));
         exit(EXIT_FAILURE);
      } else if (pid > 0) {
         // parent exiting
         exit(EXIT_SUCCESS);
      }

      // set umask for created files
      umask(0);

      // attempt to fork our own session id
      pid_t sid = setsid();
      if (sid < 0) {
         log_send(mainlog, LOG_CRIT, "daemonize: Unable to create new SID for child process: %d (%s)", errno, strerror(errno));
         exit(EXIT_FAILURE);
      }
   }

   // save pid file
   pidfd = open(pidfile, O_RDWR | O_CREAT | O_SYNC, 0600);
   if (pidfd == -1) {
      log_send(mainlog, LOG_CRIT, "daemonize: opening pid file %s failed: %d (%s)", pidfile, errno, strerror(errno));
      exit(EXIT_FAILURE);
   }

   // try to lock the pid file, so we can ensure only one instance runs
   if (lockf(pidfd, F_TLOCK, 0) != 0) {
      log_send(mainlog, LOG_CRIT, "daemonize: failed to lock pid file %s: %d (%s)", pidfile, errno, strerror(errno));
      unlink(pidfile);
      exit(EXIT_FAILURE);
   }

   // Print the process id to pidfd
   char buf[10];
   memset(buf, 0, 10);
   sprintf(buf, "%d\n", getpid());
   write(pidfd, buf, strlen(buf));

   // only close stdio if daemonizing
   if (dict_get_bool(runtime_cfg, "daemonize", false)) {
      // close stdio
      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);
   }

   return 0;
}

void fini(int status) {
   fprintf(stderr, "shutting down: %d", status);
//   dump_statistics(cfg->Get("path.statsfile", NULL));
   close(pidfd);
   pidfd = -1;
   unlink(pidfile);
   exit(status);
}

// Catch signals
static void sighandler(int signum) {
   fprintf(stderr, "caught signal %d...", signum);
   switch(signum) {
      // Convenience signals
      case SIGHUP:
         fprintf(stderr, "Reloading!");
         break;
      case SIGUSR1:
         fprintf(stderr, "Dumping database to disk");
         break;
      case SIGUSR2:
         fprintf(stderr, "Dumping statistics to disk");
//         dump_statistics(cfg->Get("path.statsfile", NULL));
         break;
      // Fatal signals
      case SIGINT:
      case SIGTERM:
      case SIGKILL:
         fini(signum);
      default:
         fprintf(stderr, "Caught unknown signal %d", signum);
         break;
   }
}

// set up signal handlers
void init_signals(void) {
   // Fatal signals
   signal(SIGINT, sighandler);
   signal(SIGTERM, sighandler);
   signal(SIGKILL, sighandler);
   // User signals
   signal(SIGHUP, sighandler);
   signal(SIGUSR1, sighandler);
   signal(SIGUSR2, sighandler);
}
