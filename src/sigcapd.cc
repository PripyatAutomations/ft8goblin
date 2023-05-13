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

int dying = 0;
const char *progname = "sigcapd";

int main(int argc, char **argv) {
   fprintf(stderr, "sigcapd doesn't exist yet but will soon!\n");
   return 0;
}
