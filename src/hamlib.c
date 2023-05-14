/*
 * here we provide for using hamlib to control an attached transceiver
 *
 * this is usually used in conjunction with alsa/pulse/sound_io...
 *
 * ft8goblin and sigcapd both use this, to allow tunable source/sinks via hamlib
 */
#include "config.h"
#include "debuglog.h"
#include "ft8goblin_types.h"
#include "hamlib.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
//
// We have to be careful to ensure all data is wrapped in a Hamlib object...
// This is the only way we can support multiple radios.
//
// from hamlib
extern int rig_raw2val();

bool hamlib_close(Hamlib *rig) {
   if (rig == NULL) {
      // XXX: deal with the error
      return false;
   }

   rig_close(rig->hamlib);

   if (rig->path != NULL)
      free(rig->path);
   free(rig);

   return true;
}

Hamlib *hamlib_open(int model, const char *path, int baudrate) {
   Hamlib *rig = NULL;

   if ((rig = malloc(sizeof(Hamlib))) == NULL) {
      fprintf(stderr, "hamlib_init: out of memory!\n");
      exit(ENOMEM);
   }
   memset(rig, 0, sizeof(Hamlib));

   // XXX: we need to store this in cfg:devices/$name/hamlib-debuglevel
   rig_set_debug(RIG_DEBUG_TRACE);

   // if model isn't set, try rigctld via localhost
   const char *mypath = NULL;
   if (model == 0) {
      model = RIG_MODEL_NETRIGCTL;
      mypath = "127.0.0.1:4532";
   } else {
      mypath = path;
   }

   // fill the structure
   rig->model = model;
   rig->hamlib = rig_init(rig->model);

   // only duplicate the path, if provided (don't call strdup(NULL)...
   if (mypath != NULL) {
      rig->path = strdup(mypath);
   }
   strncpy(rig->hamlib->state.rigport.pathname, mypath, HAMLIB_FILPATHLEN - 1);
   rig->hamlib->state.rigport.parm.serial.rate = baudrate;
   rig->retcode = rig_open(rig->hamlib);

   if (rig->retcode != RIG_OK) {
      rig_debug(RIG_DEBUG_ERR, "hamlib_init: rig_open failed %s\n", rigerror(rig->retcode));
      free(rig);
      return NULL;
   }

   rig->info_buf = (char *)rig_get_info(rig->hamlib);

   log_send(mainlog, LOG_NOTICE, "Opening rig %s", rig->info_buf);

   if (rig->hamlib->caps->rig_model == RIG_MODEL_NETRIGCTL) {
      rig->status = rig_set_vfo_opt(rig->hamlib, 1);

      if (rig->status != RIG_OK) {
         log_send(mainlog, LOG_CRIT, "hamlib_init: set_vfo_opt failed?? Err=%s", rigerror(rig->status));
         free(rig);
         return NULL;
      }
   }
   rig->status = rig_get_freq(rig->hamlib, RIG_VFO_CURR, &rig->freq);
   if (rig->status != RIG_OK) {
      log_send(mainlog, LOG_CRIT, "hamlib_init: Get freq failed?? Err=%s", rigerror(rig->status));
   }
   log_send(mainlog, LOG_NOTICE, "hamlib_init: VFO freq. = %.1f Hz\n", rig->freq);

   rig->status = rig_get_mode(rig->hamlib, RIG_VFO_CURR, &rig->mode, &rig->width);
   if (rig->status != RIG_OK) {
      log_send(mainlog, LOG_CRIT, "Get mode failed?? Err=%s\n", rigerror(rig->status));
   }

   log_send(mainlog, LOG_NOTICE, "Current mode = 0x%lX = %s, width = %ld\n", rig->mode, rig_strrmode(rig->mode), rig->width);
   rig->status = rig_get_level(rig->hamlib, RIG_VFO_CURR, RIG_LEVEL_RFPOWER, &rig->power);

   if (rig->status != RIG_OK) {
      rig_debug(RIG_DEBUG_ERR, "hamlib_init: error rig_get_level: %s\n", rigerror(rig->status));
   }
   log_send(mainlog, LOG_NOTICE, "RF Power relative setting = %.3f (0.0 - 1.0)\n", rig->power.f);

   rig->status = rig_power2mW(rig->hamlib, &rig->mwpower, rig->power.f, rig->freq, rig->mode);
   if (rig->status != RIG_OK) {
      rig_debug(RIG_DEBUG_ERR, "hamlib_init: error rig_get_level: %s\n", rigerror(rig->status));
   }
   log_send(mainlog, LOG_NOTICE, "RF Power calibrated = %.1f Watts\n", rig->mwpower / 1000.);

   /* Raw and calibrated S-meter values */
   rig->status = rig_get_level(rig->hamlib, RIG_VFO_CURR, RIG_LEVEL_RAWSTR, &rig->raw_strength);
   if (rig->status != RIG_OK) {
      rig_debug(RIG_DEBUG_ERR, "hamlib_init: error rig_get_level: %s\n", rigerror(rig->status));
   }
   log_send(mainlog, LOG_NOTICE, "Raw receive strength = %d\n", rig->raw_strength.i);
   rig->s_meter = rig_raw2val(rig->raw_strength.i, &rig->hamlib->caps->str_cal);
   log_send(mainlog, LOG_NOTICE, "S-meter value = %.2f dB relative to S9\n", rig->s_meter);

   /* now try using RIG_LEVEL_STRENGTH itself */
   rig->status = rig_get_strength(rig->hamlib, RIG_VFO_CURR, &rig->strength);

   if (rig->status != RIG_OK) {
      rig_debug(RIG_DEBUG_ERR, "hamlib_init: error rig_get_level: %s\n", rigerror(rig->status));
   }
   log_send(mainlog, LOG_NOTICE, "LEVEL_STRENGTH returns %d\n", rig->strength.i);

   const freq_range_t *range = rig_get_range(&rig->hamlib->state.rx_range_list[0],
                               14074000, RIG_MODE_USB);

   if (rig->status != RIG_OK) {
      rig_debug(RIG_DEBUG_ERR, "hamlib_init: error rig_get_range: %s\n", rigerror(rig->status));
   }

   if (range) {
      char vfolist[256];
//      rig_sprintf_vfo(vfolist, sizeof(vfolist), rig->hamlib->state.vfolist);
      snprintf(vfolist, sizeof(vfolist), "%d", rig->hamlib->state.vfo_list);
      log_send(mainlog, LOG_NOTICE, "Range start=%"PRIfreq", end=%"PRIfreq", low_power=%d, high_power=%d, vfos=%s\n",
            range->startf, range->endf, range->low_power, range->high_power, vfolist);
   } else {
      log_send(mainlog, LOG_NOTICE, "Not rx range list found\n");
   }

   return rig;
}

