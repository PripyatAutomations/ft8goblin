#if	!defined(_sql_h)
#define	_sql_h
#include <sqlite3.h>
#include <spatialite.h>

#ifdef __cplusplus
extern "C" {
#endif
    typedef struct Database {
       enum {
          DB_NONE = 0,
          DB_SQLITE3,
          DB_POSTGRES,
          DB_MYSQL
       } db_type;

       struct {
         sqlite3 *sqlite3;
       } hndl;
    } Database;

    extern Database *sql_open(const char *path);
    extern bool sql_close(Database *db);

#ifdef __cplusplus
};
#endif
#endif	// !defined(_sql_h)
