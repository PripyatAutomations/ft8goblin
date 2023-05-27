#if	!defined(_hamlib_h)
#define	_hamlib_h
#include <hamlib/amplifier.h>
#include <hamlib/rig.h>
#include <hamlib/rotator.h>

// ampclass.h
// rigclass.h
// rotclass.h

#ifdef __cplusplus
extern "C" {
#endif
   typedef struct Hamlib {
      RIG 		*hamlib;
      char		*path;
      int		model;
      freq_t	freq;
      value_t	raw_strength, power, strength;
      char		*info_buf;
      float	s_meter;
      int		status, retcode;
      unsigned int	mwpower;
      rmode_t	mode;
      pbwidth_t	width;
   } Hamlib;
   extern bool hamlib_close(Hamlib *rig);
   extern Hamlib *hamlib_open(int model, const char *path, int baudrate);
#ifdef __cplusplus
};
#endif

#endif	// !defined(_hamlib_h)
