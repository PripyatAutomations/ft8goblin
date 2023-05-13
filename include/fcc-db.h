#include "config.h"
#include "sql.h"
#include "ft8goblin_types.h"

#if	!defined(_fcc_db_h)
#define	_fcc_db_h

#ifdef __cplusplus
extern "C" {
#endif

    extern calldata_t *uls_lookup_callsign(const char *callsign);
#ifdef __cplusplus
};
#endif

#endif	// !defined(_fcc_db_h)
