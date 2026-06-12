#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── Power & Roots ───────────────────────────────────────────────────────────

double lucis_sqrt(double x);
double lucis_cbrt(double x);
double lucis_pow(double base, double exp);
double lucis_hypot(double a, double b);

// ── Exponential & Log ───────────────────────────────────────────────────────

double lucis_exp(double x);
double lucis_exp2(double x);
double lucis_ln(double x);
double lucis_log2(double x);
double lucis_log10(double x);

// ── Trigonometry ────────────────────────────────────────────────────────────

double lucis_sin(double x);
double lucis_cos(double x);
double lucis_tan(double x);
double lucis_asin(double x);
double lucis_acos(double x);
double lucis_atan(double x);
double lucis_atan2(double y, double x);

// ── Hyperbolic ──────────────────────────────────────────────────────────────

double lucis_sinh(double x);
double lucis_cosh(double x);
double lucis_tanh(double x);

// ── Rounding ────────────────────────────────────────────────────────────────

double lucis_ceil(double x);
double lucis_floor(double x);
double lucis_round(double x);
double lucis_trunc(double x);

// ── Interpolation ───────────────────────────────────────────────────────────

double lucis_lerp(double a, double b, double t);
double lucis_map(double val, double inMin, double inMax,
                  double outMin, double outMax);

// ── Conversion ──────────────────────────────────────────────────────────────

double lucis_toRadians(double deg);
double lucis_toDegrees(double rad);

// ── Checks ──────────────────────────────────────────────────────────────────

int lucis_isNaN(double x);
int lucis_isInf(double x);
int lucis_isFinite(double x);

// ── Polymorphic: abs ────────────────────────────────────────────────────────

int32_t  lucis_abs_i32(int32_t x);
int64_t  lucis_abs_i64(int64_t x);
float    lucis_abs_f32(float x);
double   lucis_abs_f64(double x);

// ── Polymorphic: min ────────────────────────────────────────────────────────

int32_t  lucis_min_i32(int32_t a, int32_t b);
int64_t  lucis_min_i64(int64_t a, int64_t b);
uint32_t lucis_min_u32(uint32_t a, uint32_t b);
uint64_t lucis_min_u64(uint64_t a, uint64_t b);
float    lucis_min_f32(float a, float b);
double   lucis_min_f64(double a, double b);

// ── Polymorphic: max ────────────────────────────────────────────────────────

int32_t  lucis_max_i32(int32_t a, int32_t b);
int64_t  lucis_max_i64(int64_t a, int64_t b);
uint32_t lucis_max_u32(uint32_t a, uint32_t b);
uint64_t lucis_max_u64(uint64_t a, uint64_t b);
float    lucis_max_f32(float a, float b);
double   lucis_max_f64(double a, double b);

// ── Polymorphic: clamp ──────────────────────────────────────────────────────

int32_t  lucis_clamp_i32(int32_t val, int32_t lo, int32_t hi);
int64_t  lucis_clamp_i64(int64_t val, int64_t lo, int64_t hi);
uint32_t lucis_clamp_u32(uint32_t val, uint32_t lo, uint32_t hi);
uint64_t lucis_clamp_u64(uint64_t val, uint64_t lo, uint64_t hi);
float    lucis_clamp_f32(float val, float lo, float hi);
double   lucis_clamp_f64(double val, double lo, double hi);

#ifdef __cplusplus
}
#endif
