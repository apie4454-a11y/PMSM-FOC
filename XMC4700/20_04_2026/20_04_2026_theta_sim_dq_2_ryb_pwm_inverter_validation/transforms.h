/*
 * transforms.h
 * 
 * Clarke and Park Coordinate Transformations
 * Ported from Simulink (17-04-2026)
 * 
 * Clarke: abc (3-phase) → αβ (stationary 2-phase)
 * Park:   αβ (stationary) → dq (rotor-aligned)
 */

#ifndef TRANSFORMS_H
#define TRANSFORMS_H

#include <math.h>

/* Transformation structures */
typedef struct {
    float alpha;
    float beta;
} Clarke_Output;

typedef struct {
    float d;
    float q;
} Park_Output;

/* ========== CLARKE TRANSFORM ========== */
/*
 * Clarke Transform: abc -> αβ (rotor-agnostic)
 * 
 * Matrix form (normalized for power invariance):
 * [α]   [  1    -1/2   -1/2  ] [a]
 * [β] = (2/3) * [0  sqrt(3)/2  -sqrt(3)/2] [b]
 *                                           [c]
 * 
 * Simplified (assuming neutral point):
 * α = (2/3) * a - (1/3) * b - (1/3) * c
 * β = (1/√3) * b - (1/√3) * c = (√3) * ((1/2)*b - (1/2)*c)
 */
static inline Clarke_Output clarke_transform(float va, float vb, float vc) {
    Clarke_Output out;
    
    /* const float INV_SQRT3 = 0.57735026919f;  // 1/√3 - unused in Clarke transform */
    const float TWO_INV_3 = 0.66666666667f;  /* 2/3 */
    const float SQRT3_2 = 0.86602540378f;    /* √3/2 */
    
    /* Standard Clarke for 3-phase power-invariant */
    out.alpha = TWO_INV_3 * va - (TWO_INV_3 / 2.0f) * vb - (TWO_INV_3 / 2.0f) * vc;
    out.beta = SQRT3_2 * vb - SQRT3_2 * vc;
    
    return out;
}

/* ========== PARK TRANSFORM ========== */
/*
 * Park Transform: αβ -> dq (rotor-aligned at angle theta)
 * 
 * Rotation matrix (counterclockwise positive):
 * [d]   [ cos(θ)  sin(θ)] [α]
 * [q] = [-sin(θ)  cos(θ)] [β]
 */
static inline Park_Output park_transform(float alpha, float beta, float theta) {
    Park_Output out;
    
    float cos_theta = cosf(theta);
    float sin_theta = sinf(theta);
    
    /* Forward Park transformation */
    out.d = cos_theta * alpha + sin_theta * beta;
    out.q = -sin_theta * alpha + cos_theta * beta;
    
    return out;
}

/* ========== INVERSE PARK TRANSFORM ========== */
/*
 * Inverse Park: dq -> αβ (back to stationary frame)
 * 
 * Inverse rotation (clockwise):
 * [α]   [ cos(θ) -sin(θ)] [d]
 * [β] = [ sin(θ)  cos(θ)] [q]
 */
static inline Clarke_Output inverse_park_transform(float d, float q, float theta) {
    Clarke_Output out;
    
    float cos_theta = cosf(theta);
    float sin_theta = sinf(theta);
    
    /* Inverse Park transformation */
    out.alpha = cos_theta * d - sin_theta * q;
    out.beta = sin_theta * d + cos_theta * q;
    
    return out;
}

/* ========== CONVENIENCE: FULL PIPELINE ========== */
/*
 * abc -> dq in one call
 * Inputs: va, vb, vc (phase voltages), theta (rotor angle)
 * Output: dq voltages
 */
static inline Park_Output abc_to_dq(float va, float vb, float vc, float theta) {
    Clarke_Output clarke_out = clarke_transform(va, vb, vc);
    Park_Output park_out = park_transform(clarke_out.alpha, clarke_out.beta, theta);
    return park_out;
}

/*
 * dq -> RYB in one call
 * Inputs: d, q (dq voltages), theta (rotor angle)
 * Outputs: vr, vy, vb (Red, Yellow, Blue phase voltages)
 * 
 * Note: Assumes balanced 3-phase (vb = -(vr+vy))
 */
typedef struct {
    float vr;  /* Red phase voltage */
    float vy;  /* Yellow phase voltage */
    float vb;  /* Blue phase voltage */
} RYB_Output;

static inline RYB_Output dq_to_ryb(float d, float q, float theta) {
    /* Step 1: Inverse Park (dq -> αβ) */
    Clarke_Output alpha_beta = inverse_park_transform(d, q, theta);
    float alpha = alpha_beta.alpha;
    float beta = alpha_beta.beta;
    
    /* Step 2: Inverse Clarke (αβ -> RYB) */
    /* Standard inverse Clarke for balanced 3-phase:
     * r = α
     * y = -α/2 + (√3/2)β
     * b = -α/2 - (√3/2)β
     */
    RYB_Output out;
    const float SQRT3_2 = 0.86602540378f;  /* √3/2 */
    
    out.vr = alpha;
    out.vy = -0.5f * alpha + SQRT3_2 * beta;
    out.vb = -0.5f * alpha - SQRT3_2 * beta;
    
    return out;
}

#endif /* TRANSFORMS_H */
