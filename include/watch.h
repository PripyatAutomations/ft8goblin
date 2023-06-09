#if	!defined(_watch_h)
#define	_watch_h

#ifdef __cplusplus
extern "C" {
#endif
   typedef enum {
      WATCH_NONE = 0,
      WATCH_CALL,
      WATCH_DXCC,
      WATCH_GRID
   } watch_type_t;

   typedef struct watch_item {
       watch_type_t	watch_type;
       char 		*watch_string;
       size_t		watch_string_sz;
       int			watch_regex_level;
   } watch_item_t;

   extern int watch_destroy(watch_item_t *w);
   extern watch_item_t *watch_create(watch_type_t watch_type, const char *watch_string, size_t watch_string_sz, int watch_regex_level);
   extern int watchlist_load(const char *path);
#ifdef __cplusplus
};
#endif

#endif	// !defined(_watch_h)
