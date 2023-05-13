#if	!defined(_ft8goblin_types_h)
#define	_ft8goblin_types_h

#define	MAX_MODES	10

#ifdef __cplusplus
extern "C" {
#endif
   typedef struct Coordinates {
      float	latitude;
      float	longitude;
   } Coordinates;

   // Only FT4 and FT8 are supported by ft8_lib, but we can talk to ardop
   typedef enum {
     TX_MODE_NONE = 0,
     TX_MODE_FT8,
     TX_MODE_FT4,
//     TX_MODE_JS8,
//     TX_MODE_PSK11,
//     TX_MODE_ARDOP_FEC,
     TX_MODE_END		// invalid, end of list marker
   } tx_mode_t;

   extern tx_mode_t tx_mode;

   // These need to move elsewhere... from ft8goblin.c
   extern const char *mode_names[MAX_MODES];
   extern const char *get_mode_name(tx_mode_t mode);
   extern void halt_tx_now(void);
   extern int view_config(void);
   extern void toggle_tx_mode(void);
   extern bool dying;			// Are we shutting down?
   extern bool tx_enabled;		// Master toggle to TX mode.
   extern bool tx_pending;		// a message has been queued for sending
   extern int tx_pending_msgs;		// how many messages are waiting to send?
   extern bool tx_even;			// TX even or odd time slot?
   extern bool cq_only;			// only show CQ + active QSOs?
   extern const char *mycall;     	// cfg:ui/mycall
   extern const char *gridsquare; 	// cfg:ui/gridsquare
   extern bool auto_cycle;

#ifdef __cplusplus
};
#endif

#endif	// !defined(_ft8goblin_types_h)
