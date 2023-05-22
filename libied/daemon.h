#if	!defined(_daemon_h)
#define	_daemon_h

#ifdef __cplusplus
extern "C" {
#endif
   extern int daemonize(void);
   extern void fini(int status);
   extern void init_signals(void);
#ifdef __cplusplus
};
#endif

#endif	// !defined(_daemon_h)
