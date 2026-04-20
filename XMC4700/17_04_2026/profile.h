/*
 * profile.h
 * 
 * Time-based signal profiles for test scenarios
 * Supports linear interpolation between breakpoints
 * 
 * Date: 18-04-2026
 */

#ifndef PROFILE_H
#define PROFILE_H

#include <stdint.h>

/* ========== PROFILE STRUCTURES ========== */

typedef struct {
    float time;      /* Time breakpoint [s] */
    float value;     /* Signal value at this time */
} ProfilePoint;

typedef struct {
    ProfilePoint *points;
    uint16_t num_points;
    float t_max;     /* Maximum time in profile */
} Profile;

/* ========== FUNCTION PROTOTYPES ========== */

/*
 * Linear interpolation of profile at time t
 * Returns value at t, or holds last value if t > t_max
 */
float profile_get_value(const Profile *profile, float t);

/*
 * Get speed reference at time t [RPM]
 * Profile: 0->1000 RPM (0-0.1s), 1000->1700 RPM (0.1-0.2s), hold 1700 RPM
 */
float profile_get_speed_ref(float t_seconds);

/*
 * Get load torque at time t [Nm]
 * Profile: Ramps 0->0.0671 (0-1s), hold 0.0671 (1-1.29s), step to 0.02 (1.29s+)
 */
float profile_get_load_torque(float t_seconds);

#endif /* PROFILE_H */
