/*
 * sigcapd attempts to provide a generic capture backend for SDRs and hamlib controlled rigs.
 *
 * For now we're focusing on RX with SDR and TX with traditional rig(s) but this should change someday?
 *
 * You need to enable ALSA or PULSE in mk/config.mk to chose which audio backends, if using hamlib!
 *
 * Sorry we depend on libuhd/gnuradio always ;(
 */
#include "config.h"
#include "debuglog.h"
#include "ft8goblin_types.h"
#include "daemon.h"
#include "dict.h"
#include "hamlib.h"
#include "maidenhead.h"
#include "ringbuffer.h"
#include "sound_io.h"
#include "sql.h"
#include "udp_src.h"
#include "uhd.h"
#include "util.h"

bool dying = false;
const char *progname = "sigcapd";
static void exit_fix_config(void) {
   printf("Please edit your config.json and try again!\n");
   exit(255);
}

int main(int argc, char **argv) {
   const char *logpath = NULL;

   if (!(cfg = load_config())) {
      exit_fix_config();
   }

   if ((logpath = dict_get(runtime_cfg, "logpath", "file://ft8goblin.log")) != NULL) {
      mainlog = log_open(logpath);
   } else {
      log_open("stderr");
   }

   while(1) {
      sleep(1);
   }
   log_close(mainlog);
   mainlog = NULL;

   return 0;
}
