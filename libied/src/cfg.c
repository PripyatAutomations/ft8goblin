/*
 * json configuration parser
 */
#include <libied/cfg.h>
#include <libied/dict.h>
#include <libied/util.h>
#include <libied/debuglog.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <yajl/yajl_tree.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>

extern char *progname;
yajl_val cfg = NULL;
dict *runtime_cfg = NULL;

yajl_val parse_config(const char *cfgfile) {
    FILE *fp = NULL;
    unsigned char *data = NULL;
    size_t rd;
    char errbuf[1024];
    yajl_val node = NULL;
    struct stat sb;
    size_t cfg_len = -1;
    errbuf[0] = 0;

    if (cfgfile == NULL) {
       fprintf(stderr, "parse_config: NULL cfgfile is not sane, exiting!\n");
       exit(255);
    }

    if (lstat(cfgfile, &sb) == -1) {
       fprintf(stderr, "parse_config: error lstat()ing cfgfile (%s): %d: %s\n", cfgfile, errno, strerror(errno));
       return NULL;
    }

    // this is gross ;)
    cfg_len = sb.st_size + 1;
    if ((data = malloc(cfg_len)) == NULL) {
       fprintf(stderr, "parse_config: error allocating memory for config parser, exiting!\n");
       exit(255);
    }
    memset(data, 0, cfg_len);

    if ((fp = fopen(cfgfile, "r")) == NULL) {
       fprintf(stderr, "parse_config: fopen(%s, \"r\") failed: %d: %s\n", cfgfile, errno, strerror(errno));
       free(data);
       fclose(fp);
       return NULL;
    }

    /* read the entire config file */
    rd = fread((void *) data, 1, cfg_len, fp);
    // XXX: this needs improved to allow multiple fread() calls, if needed.. though config really shouldn't ever get that big....

    /* file read error handling */
    if (rd == 0 && !feof(fp)) {
       fprintf(stderr, "parse_config: error encountered on file read\n");
       free(data);
       fclose(fp);
       return NULL;
    } else if (rd < (cfg_len - 1)) {
       fprintf(stderr, "parse_config: config file too big to read in one fread, this isn't yet supported ;(\n");
       free(data);
       fclose(fp);
       return NULL;
    }

    // close the file as we no longer need it...
    fclose(fp);
    fp = NULL;

    /* we have the whole config file in memory.  let's parse it ... */
    node = yajl_tree_parse((const char *) data, errbuf, sizeof(errbuf) -1);

    /* parse error handling */
    if (node == NULL) {
        fprintf(stderr, "parse_config(%s): parse_error: ", cfgfile);

        if (strlen(errbuf))
           fprintf(stderr, " %s", errbuf);
        else
           fprintf(stderr, "unknown error");

        fprintf(stderr, "\n");

        free(data);
        return NULL;
    }

    free(data);
    return node;
}

int free_config(yajl_val node) {
    yajl_tree_free(node);

    return 0;
}

//
// Try to find a configuration file and load it!
//
yajl_val load_config(void) {
   yajl_val rv = NULL;
   char buf[PATH_MAX + 1];

   // try the source directory first, in case they're trying new version out..
   memset(buf, 0, PATH_MAX + 1);
   snprintf(buf, PATH_MAX, "etc/config.json");

   memset(buf, 0, PATH_MAX + 1);
   snprintf(buf, PATH_MAX, "etc/config.json");
   if (is_file(buf)) {
      if ((rv = parse_config(buf)) != NULL) {
//         fprintf(stdout, "Using configuration in %s\n", buf);
      }
   }

   memset(buf, 0, PATH_MAX + 1);
   snprintf(buf, PATH_MAX, "%s.json", progname);
   if (is_file(buf) && (rv = parse_config(buf)) != NULL) {
//         fprintf(stdout, "Using configuration in %s\n", buf);
   }

   // try homedir
   memset(buf, 0, PATH_MAX + 1);
   snprintf(buf, PATH_MAX, "%s/.%s/config.json", getenv("HOME"), progname);
   if (is_file(buf) && (rv = parse_config(buf)) != NULL) {
//         fprintf(stdout, "Using configuration in %s\n", buf);
   }

   // and then the global directory, if config in pwd wasn't found...
   memset(buf, 0, PATH_MAX + 1);
   snprintf(buf, PATH_MAX, "/etc/%s/config.json", progname);
   if (is_file(buf) && (rv = parse_config(buf)) != NULL) {
//         fprintf(stdout, "Using configuration in %s\n", buf);
   }

   if (rv == NULL) {
      fprintf(stdout, "No configuration found\n");
      exit(1);
   }

   if (cfg == NULL) {
      cfg = rv;
   }

   // initialize the runtime configuration if it hasn't been done yet...
   if (runtime_cfg == NULL) {
      runtime_cfg = dict_new();

      char buf[4096], *bp = NULL;;
      // find the logpath...
      memset(buf, 0, 4096);
      snprintf(buf, 4096, "logging/%s-logpath", progname);
      bp = (char *)cfg_get_str(cfg, buf);

      if (bp != NULL) {
         dict_add(runtime_cfg, "logpath", bp);
      } else {
         memset(buf, 0, sizeof(buf));
         snprintf(buf, sizeof(buf), "file://logs/%s.log", progname);
         dict_add(runtime_cfg, "logpath", buf);
         fprintf(stderr, "load_config: defaulting logpath to %s\n", buf);
      }
      // find the logpath...
      memset(buf, 0, 4096);
      snprintf(buf, 4096, "logging/%s-loglevel", progname);
      bp = (char *)cfg_get_str(cfg, buf);

      if (bp != NULL) {
         dict_add(runtime_cfg, "loglevel", bp);
      } else {
         dict_add(runtime_cfg, "loglevel", "debug");
         log_send(mainlog, LOG_DEBUG, "You are seeing DEBUG level output because you have not configured loglevel for this service. Please edit config.json and set logger/%s-loglevel appropriately to reduce the log level for better performance.", progname);
      }

      // repeat for pidfile...
      memset(buf, 0, 4096);
      snprintf(buf, 4096, "logging/%s-pidfile", progname);
      bp = (char *)cfg_get_str(cfg, buf);

      if (bp != NULL) {
         dict_add(runtime_cfg, "pidfile", bp);
      } else {
         memset(buf, 0, 4096);
         snprintf(buf, 4096, "%s.pid", progname);
         dict_add(runtime_cfg, "pidfile", bp);
      }
   }

   return rv;
}

// This function shouldn't be exported
static char **cfg_mkpath(char *path, char **rpath) {
   if (rpath == NULL) {
      fprintf(stderr, "cfg_mkpath: rpath MUST not be NULL\n");
      return NULL;
   }

   if (path == NULL) {
      fprintf(stderr, "cfg_mkpath: calling with a NULL path makes no sense!\n");
      return NULL;
   }

   // zero out the array
   memset(rpath, 0, sizeof(&rpath));

   char *rp = path;
   char *p = NULL;
   int ai = 0;

   if ((p = strtok_r(path, "/.", &rp)) == NULL) {
      fprintf(stderr, "cfg_mkpath: error parsing path %s\n", path);
      return NULL;
   }
   rpath[ai] = p;
   ai++;

   while (1) {
      if ((p = strtok_r(NULL, "/.", &rp)) == NULL) {
         break;
      }
      rpath[ai] = p;
      ai++;
   }

   // make sure last item is NULL
   rpath[ai] = NULL;
   return rpath;
}

int cfg_get_int(yajl_val cfg, const char *path) {
   int rv = -1;

   if (path == NULL) {
      fprintf(stderr, "cfg_get_int: called with path == NULL, bailing!");
      return rv;
   }

   // rpath should be constructed by calling cfg_mkpath()
   char *rpath[PATHMAX_JSON];
   char *cpath = strdup(path);
   cfg_mkpath(cpath, rpath);
   yajl_val v = yajl_tree_get(cfg, (const char **)rpath, yajl_t_number);
   free(cpath);

   if (v) {
      rv = YAJL_GET_INTEGER(v);
//      fprintf(stderr, "%s: <%s> = %d\n", __FUNCTION__, path, rv);
   } else {
      log_send(mainlog, LOG_WARNING, "cfg_get_int: no such node %s. check your configuration file!", path);
   }

   return rv;
}

const char *cfg_get_str(yajl_val cfg, const char *path) {
   char *rv = NULL;

   // rpath should be constructed by calling cfg_mkpath
   char *rpath[PATHMAX_JSON];
   char *cpath = strdup(path);
   cfg_mkpath(cpath, rpath);
   yajl_val v = yajl_tree_get(cfg, (const char **)rpath, yajl_t_string);
   free(cpath);

   if (v) {
      rv = YAJL_GET_STRING(v);
//      fprintf(stderr, "cfg_get_str: <%s> = %s\n", path, rv);
   } else {
      log_send(mainlog, LOG_WARNING, "cfg_get_str: no such node %s. check your configuration file!", path);
   }

   return rv;
}
