#if	!defined(_util_h)
#define	_util_h

#ifdef __cplusplus
extern "C" {
#endif
   extern void toggle(int *v);
   extern int is_link(const char *path);
   extern int is_dir(const char *path);
   extern int is_file(const char *path);
   extern int is_fifo(const char *path);
#ifdef __cplusplus
};
#endif
#endif	// !defined(_util_h)
