#if	!defined(_qrz_xml_h)
#define _qrz_xml_h

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

   typedef enum callsign_datasrc {
      DATASRC_NONE = 0,
      DATASRC_ULS,
      DATASRC_QRZ
   } callsign_datasrc_t;

   typedef struct qrz_callsign {
      callsign_datasrc_t origin;			// origin of the data
      int		cached;				// did this result come from cache?
      char		callsign[MAX_CALLSIGN];		// callsign
      char		query_callsign[MAX_CALLSIGN];	// queried callsign (the one sent in the request)
      char		*aliases[MAX_QRZ_ALIASES];	// array of alternate callsigns, these MUST be free()d
      int		alias_count;			// how many alternate callsigns were returned?
      char		dxcc[5];			// DXCC country code (3 char)
      char		first_name[MAX_FIRSTNAME],	// first name
                        last_name[MAX_LASTNAME];	// last name
      char		address1[MAX_ADDRESS_LEN];	// address line 1
      char		address2[MAX_ADDRESS_LEN];	// address line 2
      char		address_attn;			// attn: line of address
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
      char		class[MAX_CLASS_LEN];		// license class
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
   } qrz_callsign_t;

   extern bool qrz_start_session(void);
   extern bool qrz_lookup_callsign(const char *callsign);
#ifdef __cplusplus
};
#endif
#endif	// !defined(_qrz_xml_h)
