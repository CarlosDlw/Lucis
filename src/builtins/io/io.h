#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ── String return type (matches lucis string ABI) ──────────────────────────
typedef struct { const char* ptr; size_t len; } lucis_io_string;

// ── Stdin — Text ────────────────────────────────────────────────────────────

lucis_io_string lucis_readLine(void);
char             lucis_readChar(void);
int64_t          lucis_readInt(void);
double           lucis_readFloat(void);
int              lucis_readBool(void);
lucis_io_string lucis_readAll(void);

// prompt variants: print prompt string, then read
lucis_io_string lucis_prompt(const char* msg, size_t msgLen);
int64_t          lucis_promptInt(const char* msg, size_t msgLen);
double           lucis_promptFloat(const char* msg, size_t msgLen);
int              lucis_promptBool(const char* msg, size_t msgLen);

// ── Stdin — Binary ──────────────────────────────────────────────────────────

uint8_t          lucis_readByte(void);

// ── Stdin — Status ──────────────────────────────────────────────────────────

int              lucis_isEOF(void);

// ── Secure Input ────────────────────────────────────────────────────────────

lucis_io_string lucis_readPassword(void);
lucis_io_string lucis_promptPassword(const char* msg, size_t msgLen);

// ── Stream Control ──────────────────────────────────────────────────────────

void             lucis_flush(void);
void             lucis_flushErr(void);

// ── Terminal Detection ──────────────────────────────────────────────────────

int              lucis_isTTY(void);
int              lucis_isStdoutTTY(void);
int              lucis_isStderrTTY(void);

// ── Vec-returning functions ─────────────────────────────────────────────────
// Forward declare vec header (defined in collections/vec.h)
typedef struct { void* ptr; size_t len; size_t cap; } lucis_io_vec_header;

void lucis_readLines(lucis_io_vec_header* out);
void lucis_readNBytes(lucis_io_vec_header* out, size_t n);

#ifdef __cplusplus
}
#endif
