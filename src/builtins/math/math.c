#include "math.h"

#include <math.h>
#include <stdlib.h>
#include <float.h>

// ── Power & Roots ───────────────────────────────────────────────────────────

double lucis_sqrt(double x)                { return sqrt(x); }
double lucis_cbrt(double x)                { return cbrt(x); }
double lucis_pow(double base, double exp)  { return pow(base, exp); }
double lucis_hypot(double a, double b)     { return hypot(a, b); }

// ── Exponential & Log ───────────────────────────────────────────────────────

double lucis_exp(double x)   { return exp(x); }
double lucis_exp2(double x)  { return exp2(x); }
double lucis_ln(double x)    { return log(x); }
double lucis_log2(double x)  { return log2(x); }
double lucis_log10(double x) { return log10(x); }

// ── Trigonometry ────────────────────────────────────────────────────────────

double lucis_sin(double x)             { return sin(x); }
double lucis_cos(double x)             { return cos(x); }
double lucis_tan(double x)             { return tan(x); }
double lucis_asin(double x)            { return asin(x); }
double lucis_acos(double x)            { return acos(x); }
double lucis_atan(double x)            { return atan(x); }
double lucis_atan2(double y, double x) { return atan2(y, x); }

// ── Hyperbolic ──────────────────────────────────────────────────────────────

double lucis_sinh(double x) { return sinh(x); }
double lucis_cosh(double x) { return cosh(x); }
double lucis_tanh(double x) { return tanh(x); }

// ── Rounding ────────────────────────────────────────────────────────────────

double lucis_ceil(double x)  { return ceil(x); }
double lucis_floor(double x) { return floor(x); }
double lucis_round(double x) { return round(x); }
double lucis_trunc(double x) { return trunc(x); }

// ── Interpolation ───────────────────────────────────────────────────────────

double lucis_lerp(double a, double b, double t) {
    return a + t * (b - a);
}

double lucis_map(double val, double inMin, double inMax,
                  double outMin, double outMax) {
    return outMin + (val - inMin) * (outMax - outMin) / (inMax - inMin);
}

// ── Conversion ──────────────────────────────────────────────────────────────

static const double LUCIS_PI = 3.14159265358979323846;

double lucis_toRadians(double deg) { return deg * (LUCIS_PI / 180.0); }
double lucis_toDegrees(double rad) { return rad * (180.0 / LUCIS_PI); }

// ── Checks ──────────────────────────────────────────────────────────────────

int lucis_isNaN(double x)    { return isnan(x) ? 1 : 0; }
int lucis_isInf(double x)    { return isinf(x) ? 1 : 0; }
int lucis_isFinite(double x) { return isfinite(x) ? 1 : 0; }

// ── Polymorphic: abs ────────────────────────────────────────────────────────

int32_t  lucis_abs_i32(int32_t x)  { return x < 0 ? -x : x; }
int64_t  lucis_abs_i64(int64_t x)  { return x < 0 ? -x : x; }
float    lucis_abs_f32(float x)    { return fabsf(x); }
double   lucis_abs_f64(double x)   { return fabs(x); }

// ── Polymorphic: min ────────────────────────────────────────────────────────

int32_t  lucis_min_i32(int32_t a, int32_t b)   { return a < b ? a : b; }
int64_t  lucis_min_i64(int64_t a, int64_t b)   { return a < b ? a : b; }
uint32_t lucis_min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }
uint64_t lucis_min_u64(uint64_t a, uint64_t b) { return a < b ? a : b; }
float    lucis_min_f32(float a, float b)       { return a < b ? a : b; }
double   lucis_min_f64(double a, double b)     { return a < b ? a : b; }

// ── Polymorphic: max ────────────────────────────────────────────────────────

int32_t  lucis_max_i32(int32_t a, int32_t b)   { return a > b ? a : b; }
int64_t  lucis_max_i64(int64_t a, int64_t b)   { return a > b ? a : b; }
uint32_t lucis_max_u32(uint32_t a, uint32_t b) { return a > b ? a : b; }
uint64_t lucis_max_u64(uint64_t a, uint64_t b) { return a > b ? a : b; }
float    lucis_max_f32(float a, float b)       { return a > b ? a : b; }
double   lucis_max_f64(double a, double b)     { return a > b ? a : b; }

// ── Polymorphic: clamp ──────────────────────────────────────────────────────

int32_t  lucis_clamp_i32(int32_t v, int32_t lo, int32_t hi)   { return v < lo ? lo : (v > hi ? hi : v); }
int64_t  lucis_clamp_i64(int64_t v, int64_t lo, int64_t hi)   { return v < lo ? lo : (v > hi ? hi : v); }
uint32_t lucis_clamp_u32(uint32_t v, uint32_t lo, uint32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }
uint64_t lucis_clamp_u64(uint64_t v, uint64_t lo, uint64_t hi) { return v < lo ? lo : (v > hi ? hi : v); }
float    lucis_clamp_f32(float v, float lo, float hi)         { return v < lo ? lo : (v > hi ? hi : v); }
double   lucis_clamp_f64(double v, double lo, double hi)      { return v < lo ? lo : (v > hi ? hi : v); }
