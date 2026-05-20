#ifndef LUX_TEST_H
#define LUX_TEST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* assertEqual variants */
void lux_assertEqualI64(int64_t a, int64_t b,
                                const char* file, size_t fileLen, int32_t line);
void lux_assertEqualF64(double a, double b,
                                const char* file, size_t fileLen, int32_t line);
void lux_assertEqualStr(const char* a, size_t alen,
                                    const char* b, size_t blen,
                                    const char* file, size_t fileLen, int32_t line);
void lux_assertEqualBool(int32_t a, int32_t b,
                                 const char* file, size_t fileLen, int32_t line);
void lux_assertEqualChar(int8_t a, int8_t b,
                                 const char* file, size_t fileLen, int32_t line);

/* assertNotEqual variants */
void lux_assertNotEqualI64(int64_t a, int64_t b,
                                    const char* file, size_t fileLen, int32_t line);
void lux_assertNotEqualF64(double a, double b,
                                    const char* file, size_t fileLen, int32_t line);
void lux_assertNotEqualStr(const char* a, size_t alen,
                                        const char* b, size_t blen,
                                        const char* file, size_t fileLen, int32_t line);
void lux_assertNotEqualBool(int32_t a, int32_t b,
                                     const char* file, size_t fileLen, int32_t line);
void lux_assertNotEqualChar(int8_t a, int8_t b,
                                     const char* file, size_t fileLen, int32_t line);

/* assertTrue / assertFalse */
void lux_assertTrue(int32_t cond,
                          const char* file, size_t fileLen, int32_t line);
void lux_assertFalse(int32_t cond,
                            const char* file, size_t fileLen, int32_t line);

/* Ordered comparisons (numeric only) */
void lux_assertGreaterI64(int64_t a, int64_t b,
                                  const char* file, size_t fileLen, int32_t line);
void lux_assertGreaterF64(double a, double b,
                                  const char* file, size_t fileLen, int32_t line);
void lux_assertLessI64(int64_t a, int64_t b,
                              const char* file, size_t fileLen, int32_t line);
void lux_assertLessF64(double a, double b,
                              const char* file, size_t fileLen, int32_t line);
void lux_assertGreaterEqI64(int64_t a, int64_t b,
                                     const char* file, size_t fileLen, int32_t line);
void lux_assertGreaterEqF64(double a, double b,
                                     const char* file, size_t fileLen, int32_t line);
void lux_assertLessEqI64(int64_t a, int64_t b,
                                 const char* file, size_t fileLen, int32_t line);
void lux_assertLessEqF64(double a, double b,
                                 const char* file, size_t fileLen, int32_t line);

/* String-specific */
void lux_assertStringContains(const char* s, size_t slen,
                                            const char* sub, size_t sublen,
                                            const char* file, size_t fileLen, int32_t line);

/* Float near-equality */
void lux_assertNear(double a, double b, double epsilon,
                          const char* file, size_t fileLen, int32_t line);

/* Utilities */
void lux_testFail(const char* msg, size_t msglen,
                        const char* file, size_t fileLen, int32_t line);
void lux_testSkip(const char* msg, size_t msglen,
                        const char* file, size_t fileLen, int32_t line);
void lux_testLog(const char* msg, size_t msglen,
                      const char* file, size_t fileLen, int32_t line);

#ifdef __cplusplus
}
#endif

#endif /* LUX_TEST_H */
