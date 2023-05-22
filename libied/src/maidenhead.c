/*
 * Utility functions for dealing with maidenhead coordinates and translating to/from WGS-84.
 */
#include <libied/cfg.h>
#include <libied/maidenhead.h>
#include <math.h>
#define RADIUS_EARTH 6371.0 // Earth's radius in kilometers
#include "./config.h"

double rad2deg(double rad) {
  return (rad * 180 / M_PI);
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

double deg2rad(double degrees) {
    return degrees * M_PI / 180.0;
}

double calculateBearing(double lat1, double lon1, double lat2, double lon2) {
    double dLon = deg2rad(lon2 - lon1);
    double y = sin(dLon) * cos(deg2rad(lat2));
    double x = cos(deg2rad(lat1)) * sin(deg2rad(lat2)) - sin(deg2rad(lat1)) * cos(deg2rad(lat2)) * cos(dLon);
    double bearing = atan2(y, x);
    bearing = fmod(bearing + 2 * M_PI, 2 * M_PI); // Convert to positive value
    bearing = bearing * 180.0 / M_PI; // Convert to degrees
    return bearing;
}

double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
    double dLat = deg2rad(lat2 - lat1);
    double dLon = deg2rad(lon2 - lon1);
    double a = sin(dLat / 2) * sin(dLat / 2) +
               cos(deg2rad(lat1)) * cos(deg2rad(lat2)) *
               sin(dLon / 2) * sin(dLon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));
    double distance = RADIUS_EARTH * c;
    return distance;
}
