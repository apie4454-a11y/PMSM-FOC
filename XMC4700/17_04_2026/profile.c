/*
 * profile.c
 * 
 * Time-based signal profiles for test scenarios
 * Linear interpolation between breakpoints
 * 
 * Date: 18-04-2026
 */

#include "profile.h"

/* ========== SPEED REFERENCE PROFILE ========== */
/*
 * Profile:
 * - 0-0.1s: Ramp 0 → 1000 RPM
 * - 0.1-0.2s: Ramp 1000 → 1700 RPM
 * - 0.2-30s: Hold 1700 RPM
 */
static const ProfilePoint speed_profile_points[] = {
    {0.0f, 0.0f},
    {0.1f, 1000.0f},
    {0.2f, 1700.0f},
    {30.0f, 1700.0f},
};

static const Profile speed_profile = {
    .points = (ProfilePoint *)speed_profile_points,
    .num_points = sizeof(speed_profile_points) / sizeof(ProfilePoint),
    .t_max = 30.0f,
};

/* ========== LOAD TORQUE PROFILE ========== */
/*
 * Profile:
 * - 0-0.4s: Ramp 0 → 0.01 Nm
 * - 0.4-0.6s: Ramp 0.01 → 0.02 Nm
 * - 0.6-0.8s: Ramp 0.02 → 0.04 Nm
 * - 0.8-1.0s: Ramp 0.04 → 0.0671 Nm
 * - 1.0-1.29s: Hold 0.0671 Nm
 * - 1.29s+: Step down to 0.02 Nm and hold
 */
static const ProfilePoint load_profile_points[] = {
    {0.0f, 0.0f},
    {0.4f, 0.01f},
    {0.6f, 0.02f},
    {0.8f, 0.04f},
    {1.0f, 0.0671f},
    {1.29f, 0.0671f},
    {1.3f, 0.02f},
    {30.0f, 0.02f},
};

static const Profile load_profile = {
    .points = (ProfilePoint *)load_profile_points,
    .num_points = sizeof(load_profile_points) / sizeof(ProfilePoint),
    .t_max = 30.0f,
};

/* ========== GENERIC PROFILE LOOKUP ========== */

/*
 * Linear interpolation helper
 * Assumes points are sorted by time
 */
float profile_get_value(const Profile *profile, float t) {
    /* Clamp t to [0, t_max] */
    if (t < 0.0f) t = 0.0f;
    if (t > profile->t_max) {
        /* Return last value */
        return profile->points[profile->num_points - 1].value;
    }
    
    /* Find the two points to interpolate between */
    for (uint16_t i = 0; i < profile->num_points - 1; i++) {
        float t0 = profile->points[i].time;
        float t1 = profile->points[i + 1].time;
        
        if (t >= t0 && t <= t1) {
            /* Interpolate between points[i] and points[i+1] */
            float v0 = profile->points[i].value;
            float v1 = profile->points[i + 1].value;
            
            if (t1 == t0) {
                /* Avoid division by zero (shouldn't happen) */
                return v0;
            }
            
            /* Linear interpolation: v = v0 + (v1-v0) * (t-t0) / (t1-t0) */
            float alpha = (t - t0) / (t1 - t0);
            return v0 + (v1 - v0) * alpha;
        }
    }
    
    /* Should not reach here if profile is well-formed */
    return profile->points[profile->num_points - 1].value;
}

/* ========== SPECIFIC PROFILE FUNCTIONS ========== */

float profile_get_speed_ref(float t_seconds) {
    return profile_get_value(&speed_profile, t_seconds);
}

float profile_get_load_torque(float t_seconds) {
    return profile_get_value(&load_profile, t_seconds);
}
