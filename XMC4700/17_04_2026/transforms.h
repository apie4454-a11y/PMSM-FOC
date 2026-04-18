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
    
    const float INV_SQRT3 = 0.57735026919f;  /* 1/√3 */
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
 * dq -> abc in one call
 * Inputs: d, q (dq voltages), theta (rotor angle)
 * Outputs: va, vb, vc (phase voltages)
 * 
 * Note: Assumes balanced 3-phase (vc = -(va+vb))
 */
typedef struct {
    float va;
    float vb;
    float vc;
} ABC_Output;

static inline ABC_Output dq_to_abc(float d, float q, float theta) {
    Clarke_Output park_inv = inverse_park_transform(d, q, theta);
    
    ABC_Output out;
    const float INV_SQRT3 = 0.57735026919f;
    const float TWO_INV_3 = 0.66666666667f;
    
    /* Inverse Clarke (less straightforward; using approximation for 3-phase) */
    /* For balanced 3-phase, if we have α, β, we can reconstruct abc */
    out.va = TWO_INV_3 * park_inv.alpha;
    out.vb = -(INV_SQRT3 / 2.0f) * park_inv.alpha + (0.5f) * park_inv.beta;
    out.vc = -(INV_SQRT3 / 2.0f) * park_inv.alpha - (0.5f) * park_inv.beta;
    
    return out;
}

#endif /* TRANSFORMS_H */
