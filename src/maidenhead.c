/*
 * Utility functions for dealing with maidenhead coordinates and translating to/from WGS-84.
 */
#include "config.h"
#include "ft8goblin_types.h"
#include "maidenhead.h"
#include "qrz-xml.h"

double deg2rad(double deg) {
  return (deg * pi / 180);
}

double rad2deg(double rad) {
  return (rad * 180 / pi);
}

/*
 * XXX: DRAGONS AHEAD! 
 * i haven't reviewed this yet. it came from chatgpt lol
 */
Coordinates maidenhead2latlon(const char *locator) {
   Coordinates c;
   memset(&c, 0, sizeof(Coordinates));
   c.latitude = (10 * (locator[1] - 'A') + (locator[3] - '0') + 0.5) / 60.0 + (locator[2] - '0') + (locator[0] - 'A') * 20.0 - 90.0;
   c.longitude = (20 * (locator[4] - 'A') + (locator[6] - '0') + 1.0) / 60.0 + (locator[5] - '0') + (locator[7] - 'A') * 5.0 - 180.0;
   return c;
}

// you should free this!
char *latlon2maidenhead(Coordinates *c) {
    char *out = malloc(MAX_GRID_LEN);
    memset(out, 0, MAX_GRID_LEN);

    int lat_square = floor((c->latitude + 90.0) / 10.0);
    int long_square = floor((c->longitude + 180.0) / 20.0);
    char square[MAX_GRID_LEN] = {(char)(long_square + 'A'), (char)(lat_square + 'A'), '\0'};
    double lat_remainder = fmod(c->latitude + 90.0, 10.0);
    double long_remainder = fmod(c->longitude + 180.0, 20.0);
    int lat_digit_1 = floor(lat_remainder);
    int long_digit_1 = floor(long_remainder / 2.0);
    int lat_digit_2 = floor((lat_remainder - lat_digit_1) * 60.0 / 5.0);
    int long_digit_2 = floor(fmod(long_remainder, 2.0) * 60.0 / 2.5);
    sprintf(square + strlen(square), "%d%d%c%c", long_digit_1, lat_digit_1, long_digit_2 + '0', lat_digit_2 + '0');
    memcpy(out, square, strlen(square));
    return out;
}
