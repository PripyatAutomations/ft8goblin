#if	!defined(_ft8goblin_types_h)
#define	_ft8goblin_types_h

#define	MAX_MODES	10

#define	MAX_CALLSIGN		32
#define	MAX_QRZ_ALIASES		10
#define	MAX_FIRSTNAME		65
#define	MAX_LASTNAME		65
#define	MAX_ADDRESS_LEN		128
#define	MAX_ZIP_LEN		12
#define	MAX_COUNTRY_LEN		64
#define	MAX_GRID_LEN		10
#define	MAX_COUNTY		65
#define	MAX_CLASS_LEN		11
#define	MAX_EMAIL		129
#define	MAX_URL			257

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

   typedef enum callsign_datasrc {
      DATASRC_NONE = 0,
      DATASRC_ULS,
      DATASRC_QRZ,
      DATASRC_CACHE					// cache with no other origin type set
   } callsign_datasrc_t;

   typedef struct calldata {
      callsign_datasrc_t origin;			// origin of the data
      bool		cached;				// did this result come from cache?
      time_t		cache_fetched, cache_expiry;	// when did we download it? when does it expire?
      char		callsign[MAX_CALLSIGN];		// callsign
      char		query_callsign[MAX_CALLSIGN];	// queried callsign (the one sent in the request)
      char		aliases[MAX_QRZ_ALIASES];	// array of alternate callsigns, these MUST be free()d
      int		alias_count;			// how many alternate callsigns were returned?
      int		dxcc;				// DXCC country code
      char		first_name[MAX_FIRSTNAME],	// first name
                        mi,				// middle initial
                        last_name[MAX_LASTNAME];	// last name
      char		address1[MAX_ADDRESS_LEN];	// address line 1
      char		address2[MAX_ADDRESS_LEN];	// address line 2
      char		address_attn[MAX_ADDRESS_LEN];	// attn: line of address
      char		state[3];			// state (US only)
      char		zip[MAX_ZIP_LEN];		// postal code
      char		country[MAX_COUNTRY_LEN];	 // country name
      int		country_code;			// DXCC entity code
      float		latitude;			// latitude
      float		longitude;			// longitude
      char		grid[MAX_GRID_LEN];		// grid square (8 max)
      char		county[MAX_COUNTY];		// county name
      char		fips[12];			// FIPS code for location
      char		land[MAX_COUNTRY_LEN];		// DXCC country name
      time_t		license_effective;		// effective date of license
      time_t		license_expiry;			// where their license expires
      char		previous_call[MAX_CALLSIGN];	// previous callsign
      char		opclass[MAX_CLASS_LEN];		// license class
      char		codes[MAX_CLASS_LEN];		// license type codes (USA)
      char		qsl_msg[1024];			// QSL manager contact info
      char		email[MAX_EMAIL];		// email address
      char		url[MAX_URL];			// web page URL
      uint64_t		qrz_views;			// total views on QRZ.com
      time_t		bio_updated;			// last time bio was updated
      char		image_url[MAX_URL];		// full url to primary image
      uint64_t		qrz_serial;			// database serial #
      char		gmt_offset[12];			// GMT offset (timezone)
      bool		observes_dst;			// do they observe DST?
      bool		accepts_esql;			// accepts eQSL?
      bool		accepts_paper_qsl;		// will return paper QSL?
      int		cq_zone;			// CQ zone
      int		itu_zone;			// ITU zone
      char		nickname[MAX_FIRSTNAME];	// nickname
   } calldata_t;

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
