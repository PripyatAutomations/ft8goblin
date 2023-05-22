/*
 * sigcapd attempts to provide a generic capture backend for SDRs and hamlib controlled rigs.
 *
 * For now we're focusing on RX with SDR and TX with traditional rig(s) but this should change someday?
 *
 * You need to enable ALSA or PULSE in mk/config.mk to chose which audio backends, if using hamlib!
 *
 * Sorry we depend on libuhd/gnuradio always ;(
 */
#include <libied/config.h>
#include <libied/debuglog.h>
#include <libied/daemon.h>
#include <libied/dict.h>
#include <libied/maidenhead.h>
#include <libied/ringbuffer.h>
#include <libied/util.h>
#include "ft8goblin_types.h"
#include "hamlib.h"
#include "sound_io.h"
#include "udp_src.h"
#include "uhd.h"

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

   if ((logpath = dict_get(runtime_cfg, "logpath", "file://sigcapd.log")) != NULL) {
      mainlog = log_open(logpath);
   } else {
      log_open("stderr");
   }

   log_send(mainlog, LOG_NOTICE, "sigcapd/%s starting up!", VERSION);

   while(1) {
      sleep(1);
   }
   log_close(mainlog);
   mainlog = NULL;

   return 0;
}
