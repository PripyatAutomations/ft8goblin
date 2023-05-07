/*
 * Support for looking up callsigns using FCC ULS (local) or QRZ XML API (paid)
 *
 * Here we lookup callsigns using FCC ULS and QRZ XML API to fill our cache
 */

// XXX: Here we need to try looking things up in the following order:
//	Cache
//	FCC ULS Database
//	QRZ XML API
//
// We then need to save it to the cache (if it didn't come from there already)
#include "config.h"
#include "qrz-xml.h"
#include "debuglog.h"
#include "ft8goblin_types.h"
#include "gnis-lookup.h"
#include "fcc-db.h"
#include "qrz-xml.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
static bool callsign_use_uls = false, callsign_use_qrz = false, callsign_initialized = false;

// common shared things for our library
const char *progname = "callsign-lookupd";
int dying = 0;

// Load the configuration (cfg_get_str(...)) into *our* configuration locals
static void callsign_setup(void) {
   callsign_initialized = true;

   // Use local ULS database?
   const char *s = cfg_get_str(cfg, "callsign-lookup/use-uls");

   if (strncasecmp(s, "true", 4) == 0) {
      callsign_use_uls = true;
   } else {
      callsign_use_uls = false;
   }

   // use QRZ XML API?
   s = cfg_get_str(cfg, "callsign-lookup/use-qrz");

   if (strncasecmp(s, "true", 4) == 0) {
      callsign_use_qrz = true;
   } else {
      callsign_use_qrz = false;
   }
}
   
// save a callsign record to the cache
bool callsign_cache_save(qrz_callsign_t *cp) {
    return false;
}

qrz_callsign_t *callsign_cache_find(const char *callsign) {
    return NULL;
}

qrz_callsign_t *callsign_lookup(const char *callsign) {
    bool from_cache = false;
    qrz_callsign_t *qr = NULL;

    if (!callsign_initialized) {
       callsign_setup();
    }

    // Look in cache
    qrz_callsign_t *cp = NULL;
    if ((cp = callsign_cache_find(callsign)) != NULL) {
       from_cache = true;
       return cp;
    }

    // XXX: nope, check FCC ULS

    // XXX: nope, check QRZ XML API
    qrz_lookup_callsign(callsign);

    // only save it in cache if it did not come from there already
    if (!from_cache) {
       callsign_cache_save(cp);
       return cp;
    }

    return NULL;
}

static void exit_fix_config(void) {
   printf("Please edit your config.json and try again!\n");
   exit(255);
}

int main(int argc, char **argv) {
   bool res = false;

   // This can't work without a valid configuration...
   if (!(cfg = load_config()))
      exit_fix_config();

   const char *logpath = dict_get(runtime_cfg, "logpath", "file://logs/callsign-lookupd.log");

   if (logpath != NULL) {
      mainlog = log_open(logpath);
   } else {
      fprintf(stderr, "logpath not found, defaulting to stderr!\n");
      mainlog = log_open("stderr");
   }
   log_send(mainlog, LOG_NOTICE, "%s starting up!", progname);

   callsign_setup();

   if (callsign_use_qrz) {
      res = qrz_start_session();

      if (res == false) {
         log_send(mainlog, LOG_CRIT, "Failed logging into QRZ! :(");
         exit(EACCES);
      } else {
         log_send(mainlog, LOG_INFO, "Succesfully logged into QRZ!");
      }
   }
   printf("200 OK %s %s ready to answer requests. QRZ: %s, ULS: %s, GNIS: %s\n", progname, VERSION, (callsign_use_qrz ? "On" : "Off"), (callsign_use_uls ? "On" : "Off"), (use_gnis ? "On" : "Off"));

   // example lookup
   callsign_lookup("W1AW");
   while(1) {
      sleep(1);
   }

   return 0;
}
