/*
 * Utility functions to make working with SQL more pleasant
 */
#include "config.h"
#include "ft8goblin_types.h"
#include "sql.h"
#include "debuglog.h"
// Include sqlite3 and spatialite extensions as they are our default database for read-only info
#include <sqlite3.h>
#include <spatialite.h>

// Include libpq for postgresql
#if	defined(USE_POSTGRESQL)
#include <postgresql/libpq-fe.h>
#endif

// Include mysql support
#if	defined(USE_MYSQL)
#endif

Database *sql_open(const char *path) {
    Database *db = NULL;

    if (path == NULL) {
       fprintf(stderr, "sql_open: path is NULL!\n");;
       return NULL;
    }

    if ((db = malloc(sizeof(Database))) == NULL) {
       fprintf(stderr, "sql_open: out of memory!\n");
       exit(ENOMEM);
    }

    // Is this sqlite3?
    if ((strncasecmp(path, "sqlite3:", 8) == 0)) {
       // open the sqlite database
       db->db_type = DB_SQLITE3;
       int rc = sqlite3_open(path + 8, &db->hndl.sqlite3);
       log_send(mainlog, LOG_DEBUG, "trying to open sqlite3 database %s", path + 8);
       if (rc != SQLITE_OK) {
          log_send(mainlog, LOG_CRIT, "Error opening calldata cache db %s: %s", path + 8, sqlite3_errmsg(db->hndl.sqlite3));
          exit(ENOSYS);
       } else {
          log_send(mainlog, LOG_INFO, "opened sqlite3 database %s succesfully", path + 8);
       }
    } else if ((strncasecmp(path, "postgres:", 8) == 0) || (strncasecmp(path, "pgsql:", 6) == 0)) {
       // open postgresql database
       db->db_type = DB_POSTGRES;
       log_send(mainlog, LOG_CRIT, "PostgreSQL support doesn't exist yet! Please fix your config and try again!");
       fprintf(stderr, "PostgreSQL support doesn't exist yet! Please fix your config and try again!");
       exit(ENOSYS);
    } else if (strncasecmp(path, "mysql:", 6) == 0) {
       // XXX: Someone should write mysql support
       db->db_type = DB_MYSQL;
       log_send(mainlog, LOG_CRIT, "MySQL support doesn't exist yet! Please fix your config and try again!");
       fprintf(stderr, "MySQL support doesn't exist yet! Please fix your config and try again!");
       exit(ENOSYS);
    }

    return db;
}

bool sql_close(Database *db) {
   if (db == NULL) {
      return false;
   }

   if (db->db_type == DB_SQLITE3) {
     sqlite3_close(db->hndl.sqlite3);
     db->hndl.sqlite3 = NULL;
   } else if (db->db_type == DB_POSTGRES) {
     // XXX: write this
   } else if (db->db_type == DB_MYSQL) {
     // XXX: write this
   }

   free(db);
   return true;
}
