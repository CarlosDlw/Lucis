#ifndef LUCIS_RANDOM_H
#define LUCIS_RANDOM_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
	const char* ptr;
	size_t len;
} lucis_random_str_result;

// seed(uint64)
void lucis_seed(uint64_t s);

// seedTime()
void lucis_seedTime(void);

// randInt() -> int64
int64_t lucis_randInt(void);

// randIntRange(int64, int64) -> int64
int64_t lucis_randIntRange(int64_t min, int64_t max);

// randUint() -> uint64
uint64_t lucis_randUint(void);

// randFloat() -> float64
double lucis_randFloat(void);

// randFloatRange(float64, float64) -> float64
double lucis_randFloatRange(double min, double max);

// randBool() -> bool
int32_t lucis_randBool(void);

// randChar() -> char
uint8_t lucis_randChar(void);

// uuid_v4() -> string
lucis_random_str_result lucis_uuid_v4(void);

#endif // LUCIS_RANDOM_H
