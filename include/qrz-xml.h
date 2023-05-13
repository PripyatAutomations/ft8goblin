#if	!defined(_qrz_xml_h)
#define _qrz_xml_h
#include "ft8goblin_types.h"

#ifdef __cplusplus
extern "C" {
#endif
   //
   typedef struct qrz_string {
     char *ptr;
     size_t len;
   } qrz_string_t;

   typedef struct qrz_session {
      char 	key[33];	// Session key
      int	count;		// how many lookups have been done today
      time_t	sub_expiration;	// when does my subscription end?
      time_t	last_rx;	// timestamp of last valid reply
      char	my_callsign[MAX_CALLSIGN]; // my callsign
      char	*last_msg;	// pointer to last informational message (must be free()d and NULLed!)
      char	*last_error;	// point to last error message (must be freed() and NULLed!)
   } qrz_session_t;

   extern bool qrz_start_session(void);
   extern calldata_t *qrz_lookup_callsign(const char *callsign);
#ifdef __cplusplus
};
#endif
#endif	// !defined(_qrz_xml_h)
