/*
 * flac-streamerd: Allows requesting (or sending) a FLAC stream of I/Q samples
 * across the network using FLAC lossless compression.
 *
 * This requests the samples from sigcapd behind the scenes which figures out which radio to use
 *
 * This won't be completed until after a lot of other stuff is done...
 */
#include <libied/cfg.h>
#include <libied/daemon.h>
#include <libied/debuglog.h>
#include <libied/dict.h>
#include <libied/ringbuffer.h>
#include <libied/util.h>
#include "ft8goblin_types.h"

const char *progname = "flac-streamerd";
bool dying = false;

int main(int argc, char **argv) {
   return 0;
}
