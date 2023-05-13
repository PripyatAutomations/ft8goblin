/*
 * flac-streamerd: Allows requesting (or sending) a FLAC stream of I/Q samples
 * across the network using FLAC lossless compression.
 *
 * This requests the samples from sigcapd behind the scenes which figures out which radio to use
 *
 * This won't be completed until after a lot of other stuff is done...
 */
#include "config.h"
#include "daemon.h"
#include "debuglog.h"
#include "dict.h"
#include "ft8goblin_types.h"
#include "ringbuffer.h"
#include "util.h"

const char *progname = "flac-streamerd";
bool dying = 0;

int main(int argc, char **argv) {
   return 0;
}
