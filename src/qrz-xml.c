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

extern char *progname;
time_t now;
static const char *qrz_user = NULL, *qrz_pass = NULL, *qrz_api_key = NULL, *qrz_api_url;
static qrz_session_t *qrz_session = NULL;
static bool already_logged_in = false;
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

bool qrz_parse_http_data(const char *buf, void *ptr) {
   char *key = NULL, *message = NULL, *error = NULL;
   uint64_t count = 0;
   time_t sub_exp_time = -1, qrz_gmtime = -1;
   char *newkey = NULL;
   char *new_calldata = NULL;

   qrz_session_t *q = qrz_session;
   qrz_callsign_t *calldata = (qrz_callsign_t *)ptr;

   if (q == NULL) {
      if ((q = malloc(sizeof(qrz_session_t))) == NULL) {
         fprintf(stderr, "qrz_start_session: out of memory!\n");
         exit(ENOMEM);
      }
      memset(q, 0, sizeof(qrz_session_t));

      if (qrz_session == NULL)
         qrz_session = q;

      q->count = -1;
      q->sub_expiration = 0;
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
         // XXX: In theory, this will be slightly less CPU cycles in the frequent case the key is unchanged.
         // We need to deal with the case of QRZ returning a new key when one expires during a lookup, however...
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

//         log_send(mainlog, LOG_DEBUG, ">>>countp: %s (%p), count_end: %s (%p)<<<", countp, countp, count_end, count_end);
//         log_send(mainlog, LOG_DEBUG, ">>>buf: %s (%d)<<<", buf, count_len);
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

      time_t mynow = time(NULL);
      if (q->sub_expiration <= mynow + 7776000) {		// 90 days
         log_send(mainlog, LOG_NOTICE, "QRZ subscription expires within 90 days (%d days).", (mynow - q->sub_expiration) / 86400);
      } else if (q->sub_expiration <= mynow + 5184000) {	// 60 days
         log_send(mainlog, LOG_NOTICE, "QRZ subscription expires within 60 days (%d days), you should consider renewing soon...", (mynow - q->sub_expiration) / 86400);
      } else if (q->sub_expiration <= mynow + 2592000) {	// 30 days
         log_send(mainlog, LOG_CRIT, "QRZ subscription expires within 30 days (%d days), you really should renew soon...", (mynow - q->sub_expiration) / 86400);
      }
      if (!already_logged_in) {
         log_send(mainlog, LOG_INFO, "Logged into QRZ. Your subscription expires %s. You've used %d queries today.", datebuf, q->count);
         already_logged_in = true;
      }
   }

   ////////////
   if (calldata != NULL) {
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
            log_send(mainlog, LOG_INFO, "Got Callsign data <%lu bytes>: %s", callsign_len, new_calldata);
            /* Here we need to break out the fields and apply them to their respective parts of the qrz_callsign_t */
            return true;
         }
      } else {
         log_send(mainlog, LOG_DEBUG, "calldata NULL");
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

   log_send(mainlog, LOG_DEBUG, "qrz:http_post: Fetching %s", url);

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
   qrz_api_key = cfg_get_str(cfg, "callsign-lookup/qrz-api-key");
   qrz_api_url = cfg_get_str(cfg, "callsign-lookup/qrz-api-url");

   // if any settings are missing cry and return error
   if (qrz_user == NULL || qrz_pass == NULL || qrz_api_key == NULL || qrz_api_url == NULL) {
      log_send(mainlog, LOG_CRIT, "please make sure callsign-lookup/qrz-username qrz-password and qrz-api-key are all set in config.json and try again!");
      return NULL;
   }

   log_send(mainlog, LOG_DEBUG, "Trying to log into QRZ XML API...");

   snprintf(buf, sizeof(buf), "%s?username=%s;password=%s;agent=%s-%s", qrz_api_url, qrz_user, qrz_pass, progname, VERSION);
   qrz_last_login_try = time(NULL);

   // send the request, once it completes, we should have all the data
   if (http_post(buf, NULL, outbuf, sizeof(outbuf)) != false) {
//      log_send(mainlog, LOG_DEBUG, "sending %lu bytes <%s> to parser", strlen(outbuf), outbuf);
      qrz_parse_http_data(outbuf, NULL);

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

bool qrz_lookup_callsign(const char *callsign) {
   char buf[32769], outbuf[32769];
   qrz_callsign_t *calldata = malloc(sizeof(qrz_callsign_t));

   if (calldata == NULL) {
      fprintf(stderr, "qrz_lookup_callsign: out of memory!\n");
      exit(ENOMEM);
   }

   if (callsign == NULL || qrz_api_url == NULL || qrz_session == NULL) {
      log_send(mainlog, LOG_WARNING, "qrz_lookup_callsign failed, XML API session is not yet active!");
      log_send(mainlog, LOG_WARNING, "callsign: %p <%s> api_url %p <%s> session <%p>", callsign, callsign, qrz_api_url, qrz_api_url, qrz_session);
      return false;
   }

   memset(buf, 0, sizeof(buf));
   snprintf(buf, sizeof(buf), "%s?s=%s;callsign=%s", qrz_api_url, qrz_session->key, callsign);

   log_send(mainlog, LOG_INFO, "looking up callsign %s via QRZ XML API", callsign);

   // 
   if (http_post(buf, NULL, outbuf, sizeof(outbuf)) != false) {
      memset(outbuf, 0, 32769);
      qrz_parse_http_data(outbuf, (void *)calldata);

//      log_send(mainlog, LOG_DEBUG, "calldata: callsign=%s", calldata.callsign);
   }

   return true;
}
