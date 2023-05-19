/*
 * Support for looking up callsigns via QRZ XML API.
 *
 * This is only useful for paid QRZ members.
 *
 * We cache results into cfg:callsign-lookup/cache-db for cfg:callsign-lookup/cache-expiry
 * (in etc/callsign-cache.db for 3 days by default)
 *
 * Reference: https://www.qrz.com/XML/current_spec.html
 * Current Version: 1.34
 */
#define	_XOPEN_SOURCE
#include "config.h"
#include "debuglog.h"
#include "ft8goblin_types.h"
#include "qrz-xml.h"
#include "sql.h"
#include <curl/curl.h>
#include <sys/param.h>
#include <string.h>
#include <time.h>
extern char *progname;
static const char *qrz_user = NULL, *qrz_pass = NULL, *qrz_api_key = NULL, *qrz_api_url;
static qrz_session_t *qrz_session = NULL;
static bool already_logged_in = false;
extern time_t now;
bool qrz_active = true;

// XXX: Add some code to support retrying login a few times, if error other than invalid credentials occurs
static int qrz_login_tries = 0, qrz_max_login_tries = 3;
static time_t qrz_last_login_try = -1;

static void qrz_init_string(qrz_string_t *s) {
  s->len = 0;
  s->ptr = malloc(s->len + 1);

  if (s->ptr == NULL) {
    fprintf(stderr, "qrz_init_string: out of memory!\n");
    exit(ENOMEM);
  }

  s->ptr[0] = '\0';
}

bool qrz_parse_http_data(const char *buf, calldata_t *calldata) {
   char *key = NULL, *message = NULL, *error = NULL;
   uint64_t count = 0;
   time_t sub_exp_time = -1, qrz_gmtime = -1;
   char *newkey = NULL;

   qrz_session_t *q = qrz_session;

   if (calldata == NULL) {
      log_send(mainlog, LOG_DEBUG, "qrz_parse_http_data called witn NULL calldata.");
      return false;
   }

   // if haven't yet allocated the qrz_session
   if (q == NULL) {
      if ((q = malloc(sizeof(qrz_session_t))) == NULL) {
         fprintf(stderr, "qrz_start_session: out of memory!\n");
         exit(ENOMEM);
      }
      memset(q, 0, sizeof(qrz_session_t));
      qrz_session = q;
      q->count = -1;
      q->sub_expiration = -1;
   }

   // set last received message time to now
   q->last_rx = time(NULL);

   // this is ugly...
   key = strstr(buf, "<Key>");
   if (key != NULL) {
      // skip opening tag...
      key += 5;
      char *key_end = strstr(key, "</Key>");
      size_t key_len = -1;

      if (key_end != NULL && key_end > key) {
         key_len = (key_end - key);

         char newkey[key_len + 1];
         memset(newkey, 0, key_len + 1);
         snprintf(newkey, key_len + 1, "%s", key);
            
//         log_send(mainlog, LOG_DEBUG, "qrz_xml_api: Got session key: %s, key_len: %lu", newkey, key_len);
         // We need to deal with the case of QRZ returning a new key when one expires during a lookup, however...
         // In theory, this will be slightly less CPU cycles in the frequent case the key is unchanged.
         if (strncmp(q->key, newkey, MAX(key_len, strlen(q->key))) != 0) {
            memset(q->key, 0, 33);
            snprintf(q->key, 33, "%s", newkey);
         }
      }
   }	// key != NULL

   char *sub_exp = strstr(buf, "<SubExp>");
   char *new_sub_exp = NULL;

   if (sub_exp != NULL) {
      // skip opening tag...
      sub_exp += 8;
      char *sub_exp_end = strstr(sub_exp, "</SubExp>");
      size_t sub_exp_len = -1;

      if (sub_exp_end != NULL && sub_exp_end > sub_exp) {
         sub_exp_len = (sub_exp_end - sub_exp);
         char new_sub_exp[sub_exp_len + 1];
         memset(new_sub_exp, 0, sub_exp_len + 1);
         snprintf(new_sub_exp, sub_exp_len + 1, "%s", sub_exp);
            
//         log_send(mainlog, LOG_DEBUG, "qrz_xml_api: Got SubExp: %s, len: %lu", new_sub_exp, sub_exp_len);

         struct tm tm;
         time_t myret = -1;
         memset(&tm, 0, sizeof(tm));
         strptime(new_sub_exp, "%a %b %d %H:%M:%S %Y", &tm);
         myret = mktime(&tm);

         qrz_session->sub_expiration = myret;
      }
   }	// sub_exp != NULL
   char *countp = strstr(buf, "<Count>");
   uint64_t new_count = 0;

   if (countp != NULL) {
      // skip opening tag...
      countp += 7;
      char *count_end = strstr(countp, "</Count>");
      size_t count_len = -1;

      if (count_end != NULL && count_end > countp) {
         count_len = (count_end - countp);
         char buf[256];
         memset(buf, 0, 256);
         snprintf(buf, count_len + 1, "%s", countp);

         int n = -1;
         n = atoi(buf);
         if (n == 0 && errno != 0) {
            // an error happened
            log_send(mainlog, LOG_CRIT, "qrz_xml_api: Got invalid response from atoi: %d: %s", errno, strerror(errno));
         } else {
            q->count = n;
//            log_send(mainlog, LOG_DEBUG, "qrz_xml_api: Got Count: %d", q->count);
         }
      }
   }	// count != NULL

   // is the session started?
   if (q->sub_expiration > 0 && q->key[0] != '\0' && q->count >= -1) {
      char datebuf[128];
      struct tm *tm;
      memset(datebuf, 0, 128);
      if ((tm = localtime(&q->sub_expiration)) == NULL) {
         log_send(mainlog, LOG_CRIT, "localtime() failed");
         exit(255);
      }

      if (strftime(datebuf, 128, "%Y/%m/%d %H:%M:%S", tm) == 0 && errno != 0) {
         log_send(mainlog, LOG_CRIT, "strftime() failed");
         exit(254);
      }

      // warn the user about upcoming QRZ subscription expiration starting at 90 days...
      if (!already_logged_in) {
         if (q->sub_expiration <= now + 7776000) {		// <= 90 days
            log_send(mainlog, LOG_NOTICE, "QRZ subscription expires within 90 days (%d days).", (now - q->sub_expiration) / 86400);
         } else if (q->sub_expiration <= now + 5184000) {	// <= 60 days
            log_send(mainlog, LOG_NOTICE, "QRZ subscription expires within 60 days (%d days), you should consider renewing soon...", (now - q->sub_expiration) / 86400);
         } else if (q->sub_expiration <= now + 2592000) {	// <= 30 days
            // XXX: this should pop up a dialog once per session to alert the user
            log_send(mainlog, LOG_CRIT, "QRZ subscription expires within 30 days (%d days), you really should renew soon...", (now - q->sub_expiration) / 86400);
         } else if (q->sub_expiration <= now + 604800) {	// <= 7 days
            // XXX: this should pop up a dialog once per session to alert the user
            log_send(mainlog, LOG_CRIT, "QRZ subscription expires within 7 days (%d days), you really should renew soon...", (now - q->sub_expiration) / 86400);
         } else {	// not expiring in the next 90 days
            log_send(mainlog, LOG_INFO, "Logged into QRZ. Your subscription expires %s. You've used %d queries.", datebuf, q->count);
            already_logged_in = true;
         }
      }
   }

   ////////////
   // XXX: Check and make sure this is wrapped in <QRZDatabase>
   char *callsign = strstr(buf, "<Callsign>");
   if (callsign != NULL) { 			// we got a valid callsign reply
      callsign += 10;
      char *callsign_end = strstr(callsign, "</Callsign>");
      size_t callsign_len = (callsign_end - callsign);

      // XXX: it should be a good bit larger if valid.. figure out minimal valid size..
      if (callsign_len >= 10) {
         char new_calldata[callsign_len + 1];
         memset(new_calldata, 0, callsign_len + 1);
         snprintf(new_calldata, callsign_len, "%s", callsign);
//         log_send(mainlog, LOG_INFO, "Got Callsign data <%lu bytes>: %s", callsign_len, new_calldata);

         // set the data source
         calldata->origin = DATASRC_QRZ;

         /* Here we need to break out the fields and apply them to their respective parts of the calldata_t */
         char *call = strstr(buf, "<call>");
         if (call != NULL) {
            call += 6;
            char *call_end = strstr(call, "</call>");
            size_t call_len = (call_end - call);
            memcpy(calldata->callsign, call, call_len);
         }

         char *dxcc = strstr(buf, "<dxcc>");
         if (dxcc != NULL) {
            dxcc += 6;
            char *dxcc_end = strstr(dxcc, "</dxcc>");
            size_t dxcc_len = (dxcc_end - dxcc);
            char dxcc_buf[dxcc_len + 1];
            memset(dxcc_buf, 0, dxcc_len + 1);
            calldata->dxcc = atoi(dxcc);
         }

         char *aliases = strstr(buf, "<aliases>");
         if (aliases != NULL) {
            aliases += 9;
            char *aliases_end = strstr(aliases, "</aliases>");
            size_t aliases_len = (aliases_end - aliases);
            memcpy(calldata->aliases, aliases, aliases_len);
         }

         char *fname = strstr(buf, "<fname>");
         if (fname != NULL) {
            fname += 7;
            char *fname_end = strstr(fname, "</fname>");
            size_t fname_len = (fname_end - fname);
            memcpy(calldata->first_name, fname, fname_len);
         }

         char *name = strstr(buf, "<name>");
         if (name != NULL) {
            name += 6;
            char *name_end = strstr(name, "</name>");
            size_t name_len = (name_end - name);
            memcpy(calldata->last_name, name, name_len);
         }

         char *addr1 = strstr(buf, "<addr1>");
         if (addr1 != NULL) {
            addr1 += 7;
            char *addr1_end = strstr(addr1, "</addr1>");
            size_t addr1_len = (addr1_end - addr1);
            memcpy(calldata->address1, addr1, addr1_len);
         }

         char *addr2 = strstr(buf, "<addr2>");
         if (addr2 != NULL) {
            addr2 += 7;
            char *addr2_end = strstr(addr2, "</addr2>");
            size_t addr2_len = (addr2_end - addr2);
            memcpy(calldata->address2, addr2, addr2_len);
         }

         char *state = strstr(buf, "<state>");
         if (state != NULL) {
            state += 7;
            char *state_end = strstr(state, "</state>");
            size_t state_len = (state_end - state);
            memcpy(calldata->state, state, state_len);
         }

         char *zip = strstr(buf, "<zip>");
         if (zip != NULL) {
            zip += 5;
            char *zip_end = strstr(zip, "</zip>");
            size_t zip_len = (zip_end - zip);
            memcpy(calldata->zip, zip, zip_len);
         }

         char *grid = strstr(buf, "<grid>");
         if (grid != NULL) {
            grid += 6;
            char *grid_end = strstr(grid, "</grid>");
            size_t grid_len = (grid_end - grid);
            memcpy(calldata->grid, grid, grid_len);
         }
 
         char *country = strstr(buf, "<country>");
         if (country != NULL) {
            country += 9;
            char *country_end = strstr(country, "</country>");
            size_t country_len = (country_end - country);
            memcpy(calldata->country, country, country_len);
         }

         char *lat = strstr(buf, "<lat>");
         if (lat != NULL) {
            lat += 5;
            char *lat_end = strstr(lat, "</lat>");
            size_t lat_len = (lat_end - lat);
            char lat_buf[lat_len + 1];
            memset(lat_buf, 0, lat_len + 1);
            memcpy(lat_buf, lat, lat_len);
            calldata->latitude = atof(lat_buf);
         } else {
            fprintf(stderr, "lat: NULL\n");
         }

         char *lon = strstr(buf, "<lon>");
         if (lon != NULL) {
            lon += 5;
            char *lon_end = strstr(lon, "</lon>");
            size_t lon_len = (lon_end - lon);
            char lon_buf[lon_len + 1];
            memset(lon_buf, 0, lon_len + 1);
            memcpy(lon_buf, lon, lon_len);
            calldata->longitude = atof(lon_buf);
         } else {
            fprintf(stderr, "lat: NULL\n");
         }

         char *county = strstr(buf, "<county>");
         if (county != NULL) {
            county += 8;
            char *county_end = strstr(county, "</county>");
            size_t county_len = (county_end - county);
            memcpy(calldata->county, county, county_len);
         }

         char *class = strstr(buf, "<class>");
         if (class != NULL) {
            class += 7;
            char *class_end = strstr(class, "</class>");
            size_t class_len = (class_end - class);
            memcpy(calldata->opclass, class, class_len);
         }

         char *codes = strstr(buf, "<codes>");
         if (codes != NULL) {
            codes += 7;
            char *codes_end = strstr(codes, "</codes>");
            size_t codes_len = (codes_end - codes);
            memcpy(calldata->codes, codes, codes_len);
         }

         char *email = strstr(buf, "<email>");
         if (email != NULL) {
            email += 7;
            char *email_end = strstr(email, "</email>");
            size_t email_len = (email_end - email);
            memcpy(calldata->email, email, email_len);
         }
         char *u_views = strstr(buf, "<u_views>");
         if (u_views != NULL) {
            u_views += 9;
            char *u_views_end = strstr(u_views, "</u_views>");
            size_t u_views_len = (u_views_end - u_views);
            char u_views_buf[u_views_len + 1];
            memset(u_views_buf, 0, u_views_len + 1);
            memcpy(u_views_buf, u_views, u_views_len);
         }

         char *efdate = strstr(buf, "<efdate>");
         if (efdate != NULL) {
            efdate += 8;
            char *efdate_end = strstr(efdate, "</efdate>");
            size_t efdate_len = (efdate_end - efdate);
            char efdate_buf[efdate_len + 1];
            struct tm tm;
            time_t eftime = -1;
            memset(efdate_buf, 0, efdate_len + 1);
            memcpy(efdate_buf, efdate, efdate_len);

            memset(&tm, 0, sizeof(struct tm));
            if ((strptime(efdate_buf, "%Y-%m-%d", &tm)) == NULL) {
               log_send(mainlog, LOG_WARNING, "parsing efdate from qrz failed: %d: %s", errno, strerror(errno));
            } else {
               eftime = mktime(&tm);
               calldata->license_effective = eftime;
            }
         }
         char *expdate = strstr(buf, "<expdate>");
         if (expdate != NULL) {
            expdate += 9;
            char *expdate_end = strstr(expdate, "</expdate>");
            size_t expdate_len = (expdate_end - expdate);
            char expdate_buf[expdate_len + 1];
            struct tm exptm;
            time_t exptime = -1;
            memset(expdate_buf, 0, expdate_len + 1);
            memcpy(expdate_buf, expdate, expdate_len);

            memset(&exptm, 0, sizeof(struct tm));
            if ((strptime(expdate_buf, "%Y-%m-%d", &exptm)) == NULL) {
               log_send(mainlog, LOG_WARNING, "parsing expdate from qrz failed: %d: %s", errno, strerror(errno));
            } else {
               exptime = mktime(&exptm);
               calldata->license_expiry = exptime;
            }
         }

         return true;
      }
   }
   // if we fell through to here, we were not succesful...
   return false;
}

// This function is called every time a read completes.
// we store the result into a qrz_string_t temporarily
// keep reading until the transfer is finished

static size_t qrz_http_post_cb(void *ptr, size_t size, size_t nmemb, qrz_string_t *s) {
   size_t new_len = s->len + size * nmemb;

   if (qrz_session != NULL) {
      qrz_session->last_rx = time(NULL);
   }

   s->ptr = realloc(s->ptr, new_len+1);

   if (s->ptr == NULL) {
     fprintf(stderr, "qrz_http_post_cb: Out of memory!\n");
     exit(ENOMEM);
   }

   memcpy(s->ptr + s->len, ptr, size * nmemb);
   s->ptr[new_len] = '\0';
   s->len = new_len;

//   log_send(mainlog, LOG_DEBUG, "qrz_http_post_cb: read (len=%lu): |%s|", s->len, s->ptr);

   return size * nmemb;
}

// We should probably move to the curl_multi_* api to avoid blocking, or maybe spawn this whole mess into a thread
bool http_post(const char *url, const char *postdata, char *buf, size_t bufsz) {
   CURL *curl;
   CURLcode res;
   qrz_string_t s;

   if (buf == NULL || url == NULL) {
      log_send(mainlog, LOG_DEBUG, "qrz: http_post called with out <%p>> || url <%p> NULL, this is incorrect!", buf, url);
      return false;
   }

   curl_global_init(CURL_GLOBAL_ALL);

   // create a curl instance
   if (!(curl = curl_easy_init())) {
      log_send(mainlog, LOG_WARNING, "qrz: http_post failed on curl_easy_init()");
      return false;
   }

   qrz_init_string(&s);
   curl_easy_setopt(curl, CURLOPT_URL, url);
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, qrz_http_post_cb);
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);

   char useragent[128];
   memset(useragent, 0, 128);
   snprintf(useragent, 128, "%s/%s", progname, VERSION);
  
   curl_easy_setopt(curl, CURLOPT_USERAGENT, useragent);
   curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

   // if we have POST data, attach it...
   if (postdata != NULL) {
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postdata);
   }

//   log_send(mainlog, LOG_DEBUG, "qrz:http_post: Fetching %s", url);

   // send the request
   res = curl_easy_perform(curl);

   if (res != CURLE_OK) {
      log_send(mainlog, LOG_CRIT, "qrz: http_post: curl_easy_perform() failed: %s", curl_easy_strerror(res));
      // free the string since the result was a failure
      free(s.ptr);
      s.ptr = NULL;
      s.len = -1;
      return false;
   } else if (s.len > 0) {
      // if anything was retrieved, send it to the parser
      snprintf(buf, bufsz, "%s", s.ptr);
   }
   free(s.ptr);
   s.ptr = NULL;
   s.len = -1;
   
   curl_easy_cleanup(curl);
   curl_global_cleanup();

   return true;
}

bool qrz_start_session(void) {
   char buf[4097];
   char outbuf[4097];

   memset(buf, 0, 4097);
   memset(outbuf, 0, 4097);

   qrz_user = cfg_get_str(cfg, "callsign-lookup/qrz-username");
   qrz_pass = cfg_get_str(cfg, "callsign-lookup/qrz-password");
//   qrz_api_key = cfg_get_str(cfg, "callsign-lookup/qrz-api-key");
   qrz_api_url = cfg_get_str(cfg, "callsign-lookup/qrz-api-url");

   // if any settings are missing cry and return error
   if (qrz_user == NULL || qrz_pass == NULL || qrz_api_url == NULL) {
      log_send(mainlog, LOG_CRIT, "please make sure callsign-lookup/qrz-username qrz-password and qrz-api-key are all set in config.json and try again!");
      return NULL;
   }

   log_send(mainlog, LOG_DEBUG, "Trying to log into QRZ XML API...");

   snprintf(buf, sizeof(buf), "%s?username=%s;password=%s;agent=%s-%s", qrz_api_url, qrz_user, qrz_pass, progname, VERSION);
   qrz_last_login_try = time(NULL);

   // send the request, once it completes, we should have all the data
   if (http_post(buf, NULL, outbuf, sizeof(outbuf)) != false) {
//      log_send(mainlog, LOG_DEBUG, "sending %lu bytes to parser <%s>", strlen(outbuf), outbuf);
      calldata_t calldata;
      qrz_parse_http_data(outbuf, &calldata);

      // reset the failure counter...
      qrz_login_tries = 0;
      return true;
   } else {
      log_send(mainlog, LOG_CRIT, "error logging into QRZ ;(");

      // log a failed attempt
      qrz_login_tries++;

      // XXX: We should check <Error> to see if it's a credentials problem...
   }
   return false;
}

calldata_t *qrz_lookup_callsign(const char *callsign) {
   char buf[32769], outbuf[32769];
   calldata_t *calldata = malloc(sizeof(calldata_t));

   if (calldata == NULL) {
      fprintf(stderr, "qrz_lookup_callsign: out of memory!\n");
      exit(ENOMEM);
   }

   if (callsign == NULL || qrz_api_url == NULL || qrz_session == NULL) {
      log_send(mainlog, LOG_WARNING, "qrz_lookup_callsign failed, XML API session is not yet active!");
      log_send(mainlog, LOG_WARNING, "callsign: %p <%s> api_url %p <%s> session <%p>", callsign, callsign, qrz_api_url, qrz_api_url, qrz_session);
      return false;
   }

   memset(calldata, 0, sizeof(calldata_t));
   memset(buf, 0, sizeof(buf));
   snprintf(buf, sizeof(buf), "%s?s=%s;callsign=%s", qrz_api_url, qrz_session->key, callsign);

   memcpy(calldata->query_callsign, callsign, MAX_CALLSIGN);
   log_send(mainlog, LOG_INFO, "looking up callsign %s via QRZ XML API", callsign);

   if (http_post(buf, NULL, outbuf, sizeof(outbuf)) != false) {
      qrz_parse_http_data(outbuf, calldata);
      if (calldata->callsign[0] != '\0') {
         log_send(mainlog, LOG_INFO, "result for callsign %s returned. cached: %s", calldata->callsign, (calldata->cached ? "true" : "false"));
      } else {
         log_send(mainlog, LOG_WARNING, "result for callsign %s returned, but calldata->callsign is NULL... wtf?", callsign);
         free(calldata);
         return NULL;
      }
   } else {
      free(calldata);
      return NULL;
   }
   return calldata;
}
