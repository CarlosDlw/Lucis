#ifndef LUCIS_TEST_H
#define LUCIS_TEST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* assertEqual variants */
void lucis_assertEqualI64(int64_t a, int64_t b,
                                const char* file, size_t fileLen, int32_t line);
void lucis_assertEqualF64(double a, double b,
                                const char* file, size_t fileLen, int32_t line);
void lucis_assertEqualStr(const char* a, size_t alen,
                                    const char* b, size_t blen,
                                    const char* file, size_t fileLen, int32_t line);
void lucis_assertEqualBool(int32_t a, int32_t b,
                                 const char* file, size_t fileLen, int32_t line);
void lucis_assertEqualChar(int8_t a, int8_t b,
                                 const char* file, size_t fileLen, int32_t line);

/* assertNotEqual variants */
void lucis_assertNotEqualI64(int64_t a, int64_t b,
                                    const char* file, size_t fileLen, int32_t line);
void lucis_assertNotEqualF64(double a, double b,
                                    const char* file, size_t fileLen, int32_t line);
void lucis_assertNotEqualStr(const char* a, size_t alen,
                                        const char* b, size_t blen,
                                        const char* file, size_t fileLen, int32_t line);
void lucis_assertNotEqualBool(int32_t a, int32_t b,
                                     const char* file, size_t fileLen, int32_t line);
void lucis_assertNotEqualChar(int8_t a, int8_t b,
                                     const char* file, size_t fileLen, int32_t line);

/* assertTrue / assertFalse */
void lucis_assertTrue(int32_t cond,
                          const char* file, size_t fileLen, int32_t line);
void lucis_assertFalse(int32_t cond,
                            const char* file, size_t fileLen, int32_t line);

/* Ordered comparisons (numeric only) */
void lucis_assertGreaterI64(int64_t a, int64_t b,
                                  const char* file, size_t fileLen, int32_t line);
void lucis_assertGreaterF64(double a, double b,
                                  const char* file, size_t fileLen, int32_t line);
void lucis_assertLessI64(int64_t a, int64_t b,
                              const char* file, size_t fileLen, int32_t line);
void lucis_assertLessF64(double a, double b,
                              const char* file, size_t fileLen, int32_t line);
void lucis_assertGreaterEqI64(int64_t a, int64_t b,
                                     const char* file, size_t fileLen, int32_t line);
void lucis_assertGreaterEqF64(double a, double b,
                                     const char* file, size_t fileLen, int32_t line);
void lucis_assertLessEqI64(int64_t a, int64_t b,
                                 const char* file, size_t fileLen, int32_t line);
void lucis_assertLessEqF64(double a, double b,
                                 const char* file, size_t fileLen, int32_t line);

/* String-specific */
void lucis_assertStringContains(const char* s, size_t slen,
                                            const char* sub, size_t sublen,
                                            const char* file, size_t fileLen, int32_t line);

/* Float near-equality */
void lucis_assertNear(double a, double b, double epsilon,
                          const char* file, size_t fileLen, int32_t line);

/* Utilities */
void lucis_testFail(const char* msg, size_t msglen,
                        const char* file, size_t fileLen, int32_t line);
void lucis_testSkip(const char* msg, size_t msglen,
                        const char* file, size_t fileLen, int32_t line);
void lucis_testLog(const char* msg, size_t msglen,
                      const char* file, size_t fileLen, int32_t line);

#ifdef __cplusplus
}
#endif

#endif /* LUCIS_TEST_H */
