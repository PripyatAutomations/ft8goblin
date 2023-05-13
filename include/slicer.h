#if	!defined(_slicer_h)
#define	_slicer_h
#include <stdbool.h>
#include <stdint.h>
#include <ctypes.h>

#define	MAX_SLICER_DEVS		12			// maximum devices to support per-slicer.
#ifdef __cplusplus
extern "C" {
#endif
   typedef enum {
      SLICER_IDLE = 0,
      SLICER_STARTING,
      SLICER_RUNNING,
      SLICER_CONNECTED,
      SLICER_ERROR
   } rf_slicer_status;
   typedef struct rf_slicer {
     float freq;					// start frequency in hz
     float width;					// width in hz
     rf_slicer_status status;				// status of this slicer
     rf_input_dev_t *devices[MAX_SLICER_DEVS];		// device to use for this
     rf_input_dev_t *active_device;			// which device is currently active or NULL?
     int  refcnt;					// how many clients are receiving this slice?
   } rf_slicer_t;

#if	defined(BUILDING_SIGCAPD)
   extern bool slicer_gc(void);				// garbage collect slicers (set STATUS_IDLE) if no clients connected
   extern rf_slicer_t *slicer_create(float freq, float width, rf_input_dev_t *devices);	// create a new slicer
   extern rf_slicer_t *slicer_find(float freq, float width);				// finds an existing slicer that covers this slice
   extern bool	       slicer_destroy(rf_slicer_t *slicer);				// remove a slicer
   extern bool         slicer_lock(rf_slicer_t *slicer);				// lock mutually exclusive lock on this slicer (during commands)
   extern bool         slicer_unlick(rf_slicer_t *slicer);				// unlock mutually exclusive lock on this slicer (after commands)
#endif	// defined(BUILDING_SIGCAPD)

#ifdef __cplusplus
};
#endif

#endif	// !defined(_slicer_h)
