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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <ev.h>
#include "debuglog.h"
#include "qrz-xml.h"
#include "ft8goblin_types.h"
#include "gnis-lookup.h"
#include "fcc-db.h"
#include "qrz-xml.h"
#include "sql.h"


#define BUFFER_SIZE 1024
typedef struct {
    char buffer[BUFFER_SIZE];
    size_t length;
} InputBuffer;


static bool callsign_use_uls = false, callsign_use_qrz = false, callsign_initialized = false, callsign_use_cache = false;
static const char *callsign_cache_db = NULL;
static time_t callsign_cache_expiry = -1;
static bool callsign_keep_stale_offline = false;
static Database *calldata_cache = NULL, *calldata_uls = NULL;

// common shared things for our library
const char *progname = "callsign-lookupd";
bool dying = 0;
time_t now = -1;

time_t timestr2time_t(const char *str) {
   time_t ret = 86400;
   return ret;
}

bool str2bool(const char *str, bool def) {
   if (strcasecmp(str, "true") == 0 || strcasecmp(str, "on") == 0 || strcasecmp(str, "yes") == 0) {
      return true;
   }

   return def;
}

// Load the configuration (cfg_get_str(...)) into *our* configuration locals
static void callsign_lookup_setup(void) {
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


   s = cfg_get_str(cfg, "callsign-lookup/cache-db");
   if (s == NULL) {
      log_send(mainlog, LOG_CRIT, "callsign_lookup_setup: Failed to find cache-db in config! Disabling cache...");
   } else {
      callsign_cache_db = s;
      callsign_cache_expiry = timestr2time_t(cfg_get_str(cfg, "callsign-lookup/cache-expiry"));
      callsign_keep_stale_offline = str2bool(cfg_get_str(cfg, "callsign-lookup/cache-keep-stale-if-offline"), false);

      if ((calldata_cache = sql_open(callsign_cache_db)) == NULL) {
         log_send(mainlog, LOG_CRIT, "callsign_lookup_setup: failed opening cache %s! Disabling caching!", callsign_cache_db);
         callsign_use_cache = false;
      } else {
         // cache database was succesfully opened
         // XXX: Detect if we need to initialize it -- does table cache exist?
         // XXX: Initialize the tables using sql in sql/cache.sql
         callsign_use_cache = true;
      }
   }
}
   
// save a callsign record to the cache
bool callsign_cache_save(calldata_t *cp) {
    return false;
}

calldata_t *callsign_cache_find(const char *callsign) {
    return NULL;
}

calldata_t *callsign_lookup(const char *callsign) {
    bool from_cache = false;
    calldata_t *qr = NULL;

    if (!callsign_initialized) {
       callsign_lookup_setup();
    }

    // Look in cache
    if ((qr = callsign_cache_find(callsign)) != NULL) {
       from_cache = true;
       return qr;
    }

    // nope, check FCC ULS
    if (qr == NULL) {
       qr = uls_lookup_callsign(callsign);
    }

    // nope, check QRZ XML API
    if (qr == NULL) {
       qr = qrz_lookup_callsign(callsign);
    }

    // only save it in cache if it did not come from there already
    if (!from_cache) {
       callsign_cache_save(qr);
       return qr;
    }

    return NULL;
}

static void exit_fix_config(void) {
   printf("Please edit your config.json and try again!\n");
   exit(255);
}

// dump all the set attributes of a calldata to the screen
bool calldata_dump(calldata_t *calldata, const char *callsign) {
   if (calldata == NULL) {
      return false;
   }

   if (calldata->callsign[0] == '\0') {
      if (calldata->query_callsign[0] == '\0') {
         fprintf(stdout, "404 NOT FOUND %lu %s\n", now, calldata->query_callsign);
         log_send(mainlog, LOG_DEBUG, "Lookup for %s failed, not found!\n", calldata->query_callsign);
      } else if (callsign != NULL) {
         fprintf(stdout, "404 NOT FOUND %lu %s\n", now, callsign);
      } else {
         log_send(mainlog, LOG_DEBUG, "Lookup failed: query_callsign unset!");
         fprintf(stdout, "404 NOT FOUND %lu\n", now);
      }
      return false;
   }

   // 200 OK N0CALL ONLINE 1683541080 QRZ
   // Lookup for N0CALL succesfully performed using QRZ online (not cached)
   // at Mon May  8 06:18:00 AM EDT 2023.
   // 200 OK N0CALL CACHE 1683541080 QRZ EXPIRES 1683800280
   // Lookup for N0CALL was answered by the cache.
   // The answer originally came from QRZ at Mon May  8 06:18:00 AM EDT 2023
   // and will expire (if we go online) at Thu May 11 06:18:00 AM EDT 2023.
   fprintf(stdout, "200 OK %s ONLINE %lu QRZ\n", calldata->callsign, time(NULL));
   fprintf(stdout, "Callsign: %s\n", calldata->callsign);

   fprintf(stdout, "Cached: %s\n", (calldata->cached ? "true" : "false"));
   if (calldata->cached) {
      fprintf(stdout, "Cache-Fetched: %lu\n", calldata->cache_fetched);
      fprintf(stdout, "Cache-Expiry: %lu\n", calldata->cache_expiry);
   }

   if (calldata->first_name[0] != '\0') {
      fprintf(stdout, "Name: %s %s\n", calldata->first_name, calldata->last_name);
   }

   if (calldata->alias_count > 0 && (calldata->aliases[0] != '\0')) {
      fprintf(stdout, "Aliases: %d: %s\n", calldata->alias_count, calldata->aliases);
   }

   if (calldata->dxcc != 0) {
      fprintf(stdout, "DXCC: %d\n", calldata->dxcc);
   }

   if (calldata->email[0] != '\0') {
      fprintf(stdout, "Email: %s\n", calldata->email);
   }

   if (calldata->grid[0] != 0) {
      fprintf(stdout, "Grid: %s\n", calldata->grid);
   }

   if (calldata->latitude != 0 && calldata->longitude != 0) {
      fprintf(stdout, "WGS-84: %f, %f\n", calldata->latitude, calldata->longitude);
   }

   if (calldata->address1[0] != '\0') {
      fprintf(stdout, "Address1: %s\n", calldata->address1);
   }

   if (calldata->address_attn[0] != '\0') {
      fprintf(stdout, "Attn: %s\n", calldata->address_attn);
   }

   if (calldata->address2[0] != '\0') {
      fprintf(stdout, "Address2: %s\n", calldata->address2);
   }

   if (calldata->state[0] != '\0') {
      fprintf(stdout, "State: %s\n", calldata->state);
   }

   if (calldata->zip[0] != '\0') {
      fprintf(stdout, "Zip: %s\n", calldata->zip);
   }

   if (calldata->county[0] != '\0') {
      fprintf(stdout, "County: %s\n", calldata->county);
   }

   if (calldata->fips[0] != '\0') {
      fprintf(stdout, "FIPS: %s\n", calldata->fips);
   }

   if (calldata->license_effective > 0) {
      struct tm *eff_tm = localtime(&calldata->license_effective);

      if (eff_tm == NULL) {
         log_send(mainlog, LOG_DEBUG, "calldata_dump: failed converting license_effective to tm");
      } else {
         char eff_buf[129];
         memset(eff_buf, 0, 129);

         size_t eff_ret = -1;
         if ((eff_ret = strftime(eff_buf, 128, "%Y/%m/%d", eff_tm)) == 0) {
            if (errno != 0) {
               log_send(mainlog, LOG_DEBUG, "calldata_dump: strfime license effective failed: %d: %s", errno, strerror(errno));
            }
         } else {
            fprintf(stdout, "License Effective: %s\n", eff_buf);
         }
      }
   } else {
      fprintf(stdout, "License Effective: UNKNOWN\n");
   }

   if (calldata->license_expiry > 0) {
      struct tm *exp_tm = localtime(&calldata->license_expiry);

      if (exp_tm == NULL) {
         log_send(mainlog, LOG_DEBUG, "calldata_dump: failed converting license_expiry to tm");
      } else {
         char exp_buf[129];
         memset(exp_buf, 0, 129);

         size_t ret = -1;
         if ((ret = strftime(exp_buf, 128, "%Y/%m/%d", exp_tm)) == 0) {
            if (errno != 0) {
               log_send(mainlog, LOG_DEBUG, "calldata_dump: strfime license expiry failed: %d: %s", errno, strerror(errno));
            }
         } else {
            fprintf(stdout, "License Expires: %s\n", exp_buf);
         }
      }
   } else {
      fprintf(stdout, "License Expires: UNKNOWN\n");
   }

   if (calldata->country[0] != '\0') {
      fprintf(stdout, "Country: %s (%d)\n", calldata->country, calldata->country_code);
   }

   return true;
}

// XXX: Create a socket IO type to pass around here
typedef struct sockio {
  char readbuf[16384];
  char writebuf[32768];
} sockio_t;

static bool parse_request(const char *line) {
//   fprintf(stderr, "stdin_cb: Parsing line: %s\n", line);
   // XXX: Here we need to parse commands from stdin
   if (strncasecmp(line, "LOOKUP ", 7) == 0) {
      const char *callsign = line + 7;

      calldata_t *calldata = callsign_lookup(callsign);

      if (calldata == NULL) {
         fprintf(stdout, "404 NOT FOUND %lu %s\n", now, callsign);
         log_send(mainlog, LOG_NOTICE, "Callsign %s was not found in enabled databases.", callsign);
         // give error status for scripts
         exit(1);
      } else {
         // Send the result
         calldata_dump(calldata, callsign);
      }
      free(calldata);
      calldata = NULL;
   } else {
      fprintf(stderr, "500 Your client sent a request I do not understand... Try again!\n");
      // XXX: we should keep track of invalid requests and eventually punt the client...
   }
   
   return false;
}

static void stdio_cb(EV_P_ ev_io *w, int revents) {
    if (EV_ERROR & revents) {
        fprintf(stderr, "Error event in stdin watcher\n");
        return;
    }

    InputBuffer *input = (InputBuffer *)w->data;
    ssize_t bytesRead = read(STDIN_FILENO, input->buffer + input->length, BUFFER_SIZE - input->length);

    if (bytesRead < 0) {
        perror("read");
        return;
    }

    if (bytesRead == 0) {
        // End of file (Ctrl+D pressed)
        ev_io_stop(EV_A, w);
        free(input);
        return;
    }

    input->length += bytesRead;

    // Process complete lines
    char *newline;
    while ((newline = strchr(input->buffer, '\n')) != NULL) {
        *newline = '\0';  // Replace newline character with null terminator
        parse_request(input->buffer);
        memmove(input->buffer, newline + 1, input->length - (newline - input->buffer));
        input->length -= (newline - input->buffer) + 1;
    }

    // If buffer is full and no newline is found, consider it an incomplete line
    if (input->length == BUFFER_SIZE) {
        fprintf(stderr, "Input buffer full, discarding incomplete line: %s\n", input->buffer);
        input->length = 0;  // Discard the incomplete line
    }
}

static void periodic_cb(EV_P_ ev_timer *w, int revents) {
   // update our shared timestamp
   now = time(NULL);
}

int main(int argc, char **argv) {
   struct ev_loop *loop = EV_DEFAULT;
   struct ev_io stdin_watcher;
   struct ev_timer periodic_watcher;
   bool res = false;
   InputBuffer *input = NULL;

   now = time(NULL);

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
   log_send(mainlog, LOG_NOTICE, "%s/%s starting up!", progname, VERSION);

   if ((input = malloc(sizeof(InputBuffer))) == NULL) {
      log_send(mainlog, LOG_CRIT, "malloc(InputBuffer): out of memory!");
      exit(ENOMEM);
   }
   memset(input, 0, sizeof(InputBuffer));

   ev_io_init(&stdin_watcher, stdio_cb, STDIN_FILENO, EV_READ);
   stdin_watcher.data = input;
   ev_io_start(loop, &stdin_watcher);

   // start our once a second periodic timer (used for housekeeping and the clock display)
   ev_timer_init(&periodic_watcher, periodic_cb, 0, 1);
   ev_timer_start(loop, &periodic_watcher);

   callsign_lookup_setup();

   if (callsign_use_qrz) {
      res = qrz_start_session();

      if (res == false) {
         log_send(mainlog, LOG_CRIT, "Failed logging into QRZ! :(");
         exit(EACCES);
      }
   }
   printf("+OK %s/%s ready to answer requests. QRZ: %s, ULS: %s, GNIS: %s\n", progname, VERSION, (callsign_use_qrz ? "On" : "Off"), (callsign_use_uls ? "On" : "Off"), (use_gnis ? "On" : "Off"));

   // if called with callsign(s) as args, look them up, return the parsed output and exit
   if (argc > 1) {
      for (int i = 1; i <= (argc - 1); i++) {
         char *callsign = argv[i];

         calldata_t *calldata = callsign_lookup(callsign);

         if (calldata == NULL) {
            fprintf(stdout, "404 NOT FOUND %lu %s\n", now, callsign);
            log_send(mainlog, LOG_NOTICE, "Callsign %s was not found in enabled databases.", callsign);
            // give error status for scripts
            exit(1);
         } else {
            // Send the result
            calldata_dump(calldata, callsign);
         }
         free(calldata);
         calldata = NULL;
      }

      dying = true;
   }

   while(!dying) {
      ev_run(loop, 0);

      // if ev loop exits, we need to die..
      dying = true;
   }

   // Close the database(s)
   if (calldata_cache != NULL) {
      sql_close(calldata_cache);
      calldata_cache = NULL;
   }

   if (calldata_uls != NULL) {
      sql_close(calldata_uls);
      calldata_uls = NULL;
   }

   //
   if (input != NULL) {
      free(input);
      input = NULL;
   }
   return 0;
}
