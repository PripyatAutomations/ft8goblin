#if	!defined(_maidenhead_h)
#define	_maidenhead_h
#include "ft8goblin_types.h"
#include "qrz-xml.h"
#include <math.h>
#if	!defined(pi)
#define pi 3.14159265358979323846
#endif


#ifdef __cplusplus
extern "C" {
#endif

extern double deg2rad(double deg);
extern double rad2deg(double rad);
extern Coordinates maidenhead2latlon(const char *locator);
extern char *latlon2maidenhead(Coordinates *c);

#ifdef __cplusplus
};
#endif

#endif	// !defined(_maidenhead_h)
