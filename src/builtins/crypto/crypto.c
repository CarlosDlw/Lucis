#include "crypto.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── helpers ──────────────────────────────────────────────────────────── */

static const char hex_lut[] = "0123456789abcdef";

static char* bytes_to_hex(const uint8_t* data, size_t n) {
    char* out = (char*)malloc(n * 2 + 1);
    for (size_t i = 0; i < n; i++) {
        out[i * 2]     = hex_lut[data[i] >> 4];
        out[i * 2 + 1] = hex_lut[data[i] & 0x0f];
    }
    out[n * 2] = '\0';
    return out;
}

/* ─── MD5 (RFC 1321) ───────────────────────────────────────────────────── */

static const uint32_t md5_k[64] = {
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,
    0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,
    0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,
    0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,
    0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,
    0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,
    0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,
    0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,
    0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};

static const uint32_t md5_s[64] = {
    7,12,17,22, 7,12,17,22, 7,12,17,22, 7,12,17,22,
    5, 9,14,20, 5, 9,14,20, 5, 9,14,20, 5, 9,14,20,
    4,11,16,23, 4,11,16,23, 4,11,16,23, 4,11,16,23,
    6,10,15,21, 6,10,15,21, 6,10,15,21, 6,10,15,21
};

static uint32_t md5_rotl(uint32_t x, uint32_t c) {
    return (x << c) | (x >> (32 - c));
}

static void md5_hash(const uint8_t* msg, size_t len, uint8_t out[16]) {
    uint32_t a0 = 0x67452301, b0 = 0xefcdab89,
             c0 = 0x98badcfe, d0 = 0x10325476;

    size_t new_len = len + 1;
    while (new_len % 64 != 56) new_len++;
    uint8_t* buf = (uint8_t*)calloc(new_len + 8, 1);
    memcpy(buf, msg, len);
    buf[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    memcpy(buf + new_len, &bits, 8);

    for (size_t off = 0; off < new_len + 8; off += 64) {
        uint32_t* m = (uint32_t*)(buf + off);
        uint32_t a = a0, b = b0, c = c0, d = d0;
        for (uint32_t i = 0; i < 64; i++) {
            uint32_t f, g;
            if (i < 16)      { f = (b & c) | (~b & d);       g = i; }
            else if (i < 32) { f = (d & b) | (~d & c);       g = (5*i+1) % 16; }
            else if (i < 48) { f = b ^ c ^ d;                g = (3*i+5) % 16; }
            else              { f = c ^ (b | ~d);             g = (7*i) % 16; }
            f = f + a + md5_k[i] + m[g];
            a = d; d = c; c = b;
            b = b + md5_rotl(f, md5_s[i]);
        }
        a0 += a; b0 += b; c0 += c; d0 += d;
    }
    free(buf);

    memcpy(out,      &a0, 4);
    memcpy(out + 4,  &b0, 4);
    memcpy(out + 8,  &c0, 4);
    memcpy(out + 12, &d0, 4);
}

/* ─── SHA-1 (FIPS 180-4) ──────────────────────────────────────────────── */

static uint32_t sha1_rotl(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static uint32_t sha1_be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

static void sha1_store_be32(uint8_t* p, uint32_t v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)v;
}

static void sha1_hash(const uint8_t* msg, size_t len, uint8_t out[20]) {
    uint32_t h0=0x67452301, h1=0xEFCDAB89, h2=0x98BADCFE,
             h3=0x10325476, h4=0xC3D2E1F0;

    size_t new_len = len + 1;
    while (new_len % 64 != 56) new_len++;
    uint8_t* buf = (uint8_t*)calloc(new_len + 8, 1);
    memcpy(buf, msg, len);
    buf[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        buf[new_len + i] = (uint8_t)(bits >> (56 - 8*i));

    for (size_t off = 0; off < new_len + 8; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = sha1_be32(buf + off + i*4);
        for (int i = 16; i < 80; i++)
            w[i] = sha1_rotl(w[i-3]^w[i-8]^w[i-14]^w[i-16], 1);

        uint32_t a=h0, b=h1, c=h2, d=h3, e=h4;
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20)      { f=(b&c)|((~b)&d);   k=0x5A827999; }
            else if (i < 40) { f=b^c^d;             k=0x6ED9EBA1; }
            else if (i < 60) { f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDC; }
            else              { f=b^c^d;             k=0xCA62C1D6; }
            uint32_t t = sha1_rotl(a,5)+f+e+k+w[i];
            e=d; d=c; c=sha1_rotl(b,30); b=a; a=t;
        }
        h0+=a; h1+=b; h2+=c; h3+=d; h4+=e;
    }
    free(buf);

    sha1_store_be32(out,    h0);
    sha1_store_be32(out+4,  h1);
    sha1_store_be32(out+8,  h2);
    sha1_store_be32(out+12, h3);
    sha1_store_be32(out+16, h4);
}

/* ─── SHA-256 (FIPS 180-4) ─────────────────────────────────────────────── */

static const uint32_t sha256_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static uint32_t sha256_rotr(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static uint32_t sha256_be32(const uint8_t* p) {
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|
           ((uint32_t)p[2]<<8)|(uint32_t)p[3];
}

static void sha256_store_be32(uint8_t* p, uint32_t v) {
    p[0]=(uint8_t)(v>>24); p[1]=(uint8_t)(v>>16);
    p[2]=(uint8_t)(v>>8);  p[3]=(uint8_t)v;
}

static void sha256_hash(const uint8_t* msg, size_t len, uint8_t out[32]) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    size_t new_len = len + 1;
    while (new_len % 64 != 56) new_len++;
    uint8_t* buf = (uint8_t*)calloc(new_len + 8, 1);
    memcpy(buf, msg, len);
    buf[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++)
        buf[new_len + i] = (uint8_t)(bits >> (56 - 8*i));

    for (size_t off = 0; off < new_len + 8; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = sha256_be32(buf + off + i*4);
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = sha256_rotr(w[i-15],7)^sha256_rotr(w[i-15],18)^(w[i-15]>>3);
            uint32_t s1 = sha256_rotr(w[i-2],17)^sha256_rotr(w[i-2],19)^(w[i-2]>>10);
            w[i] = w[i-16]+s0+w[i-7]+s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],
                 e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = sha256_rotr(e,6)^sha256_rotr(e,11)^sha256_rotr(e,25);
            uint32_t ch = (e&f)^((~e)&g);
            uint32_t t1 = hh+S1+ch+sha256_k[i]+w[i];
            uint32_t S0 = sha256_rotr(a,2)^sha256_rotr(a,13)^sha256_rotr(a,22);
            uint32_t maj = (a&b)^(a&c)^(b&c);
            uint32_t t2 = S0+maj;
            hh=g; g=f; f=e; e=d+t1;
            d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
        h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    free(buf);

    for (int i = 0; i < 8; i++)
        sha256_store_be32(out + i*4, h[i]);
}

/* ─── SHA-512 (FIPS 180-4) ─────────────────────────────────────────────── */

static const uint64_t sha512_k[80] = {
    0x428a2f98d728ae22ULL,0x7137449123ef65cdULL,
    0xb5c0fbcfec4d3b2fULL,0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL,0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL,0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL,0x12835b0145706fbeULL,
    0x243185be4ee4b28cULL,0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL,0x80deb1fe3b1696b1ULL,
    0x9bdc06a725c71235ULL,0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL,0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL,0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL,0x4a7484aa6ea6e483ULL,
    0x5cb0a9dcbd41fbd4ULL,0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL,0xa831c66d2db43210ULL,
    0xb00327c898fb213fULL,0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL,0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL,0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL,0x2e1b21385c26c926ULL,
    0x4d2c6dfc5ac42aedULL,0x53380d139d95b3dfULL,
    0x650a73548baf63deULL,0x766a0abb3c77b2a8ULL,
    0x81c2c92e47edaee6ULL,0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL,0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL,0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL,0xd69906245565a910ULL,
    0xf40e35855771202aULL,0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL,0x1e376c085141ab53ULL,
    0x2748774cdf8eeb99ULL,0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL,0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL,0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL,0x78a5636f43172f60ULL,
    0x84c87814a1f0ab72ULL,0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL,0xa4506cebde82bde9ULL,
    0xbef9a3f7b2c67915ULL,0xc67178f2e372532bULL,
    0xca273eceea26619cULL,0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL,0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL,0x0a637dc5a2c898a6ULL,
    0x113f9804bef90daeULL,0x1b710b35131c471bULL,
    0x28db77f523047d84ULL,0x32caab7b40c72493ULL,
    0x3c9ebe0a15c9bebcULL,0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL,0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL,0x6c44198c4a475817ULL
};

static uint64_t sha512_rotr(uint64_t x, int n) {
    return (x >> n) | (x << (64 - n));
}

static uint64_t sha512_be64(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v = (v << 8) | p[i];
    return v;
}

static void sha512_store_be64(uint8_t* p, uint64_t v) {
    for (int i = 7; i >= 0; i--) {
        p[i] = (uint8_t)v;
        v >>= 8;
    }
}

static void sha512_hash(const uint8_t* msg, size_t len, uint8_t out[64]) {
    uint64_t h[8] = {
        0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL,
        0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
        0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL,
        0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
    };

    /* pad: msg + 0x80 + zeros + 128-bit length (big-endian) */
    size_t new_len = len + 1;
    while (new_len % 128 != 112) new_len++;
    uint8_t* buf = (uint8_t*)calloc(new_len + 16, 1);
    memcpy(buf, msg, len);
    buf[len] = 0x80;
    /* store length in bits as 128-bit big-endian (upper 64 bits = 0) */
    uint64_t bits = (uint64_t)len * 8;
    sha512_store_be64(buf + new_len + 8, bits);

    for (size_t off = 0; off < new_len + 16; off += 128) {
        uint64_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = sha512_be64(buf + off + i*8);
        for (int i = 16; i < 80; i++) {
            uint64_t s0 = sha512_rotr(w[i-15],1)^sha512_rotr(w[i-15],8)^(w[i-15]>>7);
            uint64_t s1 = sha512_rotr(w[i-2],19)^sha512_rotr(w[i-2],61)^(w[i-2]>>6);
            w[i] = w[i-16]+s0+w[i-7]+s1;
        }
        uint64_t a=h[0],b=h[1],c=h[2],d=h[3],
                 e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 80; i++) {
            uint64_t S1 = sha512_rotr(e,14)^sha512_rotr(e,18)^sha512_rotr(e,41);
            uint64_t ch = (e&f)^((~e)&g);
            uint64_t t1 = hh+S1+ch+sha512_k[i]+w[i];
            uint64_t S0 = sha512_rotr(a,28)^sha512_rotr(a,34)^sha512_rotr(a,39);
            uint64_t maj = (a&b)^(a&c)^(b&c);
            uint64_t t2 = S0+maj;
            hh=g; g=f; f=e; e=d+t1;
            d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d;
        h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    free(buf);

    for (int i = 0; i < 8; i++)
        sha512_store_be64(out + i*8, h[i]);
}

/* ─── public API ───────────────────────────────────────────────────────── */

lucis_crypto_str_result lucis_md5String(const char* s, size_t slen) {
    uint8_t digest[16];
    md5_hash((const uint8_t*)s, slen, digest);
    char* hex = bytes_to_hex(digest, 16);
    return (lucis_crypto_str_result){ hex, 32 };
}

lucis_crypto_str_result lucis_sha1String(const char* s, size_t slen) {
    uint8_t digest[20];
    sha1_hash((const uint8_t*)s, slen, digest);
    char* hex = bytes_to_hex(digest, 20);
    return (lucis_crypto_str_result){ hex, 40 };
}

lucis_crypto_str_result lucis_sha256String(const char* s, size_t slen) {
    uint8_t digest[32];
    sha256_hash((const uint8_t*)s, slen, digest);
    char* hex = bytes_to_hex(digest, 32);
    return (lucis_crypto_str_result){ hex, 64 };
}

lucis_crypto_str_result lucis_sha512String(const char* s, size_t slen) {
    uint8_t digest[64];
    sha512_hash((const uint8_t*)s, slen, digest);
    char* hex = bytes_to_hex(digest, 64);
    return (lucis_crypto_str_result){ hex, 128 };
}

/* ── Vec<uint8> variants ───────────────────────────────────────── */

lucis_crypto_str_result lucis_md5Bytes(const lucis_crypto_vec_header* data) {
    return lucis_md5String((const char*)data->ptr, data->len);
}

lucis_crypto_str_result lucis_sha1Bytes(const lucis_crypto_vec_header* data) {
    return lucis_sha1String((const char*)data->ptr, data->len);
}

lucis_crypto_str_result lucis_sha256Bytes(const lucis_crypto_vec_header* data) {
    return lucis_sha256String((const char*)data->ptr, data->len);
}

lucis_crypto_str_result lucis_sha512Bytes(const lucis_crypto_vec_header* data) {
    return lucis_sha512String((const char*)data->ptr, data->len);
}

void lucis_hmacSha256(lucis_crypto_vec_header* out,
                       const lucis_crypto_vec_header* key,
                       const lucis_crypto_vec_header* data) {
    const uint8_t* keyBytes = (const uint8_t*)key->ptr;
    size_t keyLen = key->len;
    const uint8_t* msgBytes = (const uint8_t*)data->ptr;
    size_t msgLen = data->len;

    uint8_t kPad[64];
    memset(kPad, 0, 64);

    if (keyLen > 64) {
        sha256_hash(keyBytes, keyLen, kPad);
        keyLen = 32;
    } else {
        memcpy(kPad, keyBytes, keyLen);
    }

    // Inner hash: SHA256(ipad || message)
    uint8_t ipad[64];
    for (int i = 0; i < 64; i++) ipad[i] = kPad[i] ^ 0x36;

    size_t innerLen = 64 + msgLen;
    uint8_t* innerBuf = (uint8_t*)malloc(innerLen);
    memcpy(innerBuf, ipad, 64);
    memcpy(innerBuf + 64, msgBytes, msgLen);

    uint8_t innerHash[32];
    sha256_hash(innerBuf, innerLen, innerHash);
    free(innerBuf);

    // Outer hash: SHA256(opad || innerHash)
    uint8_t opad[64];
    for (int i = 0; i < 64; i++) opad[i] = kPad[i] ^ 0x5c;

    uint8_t outerBuf[64 + 32];
    memcpy(outerBuf, opad, 64);
    memcpy(outerBuf + 64, innerHash, 32);

    uint8_t* result = (uint8_t*)malloc(32);
    sha256_hash(outerBuf, 96, result);

    out->ptr = result;
    out->len = 32;
    out->cap = 32;
}

void lucis_randomBytes(lucis_crypto_vec_header* out, size_t n) {
    uint8_t* buf = (uint8_t*)malloc(n);

    FILE* f = fopen("/dev/urandom", "rb");
    if (f) {
        size_t got = fread(buf, 1, n, f);
        fclose(f);
        (void)got;
    }

    out->ptr = buf;
    out->len = n;
    out->cap = n;
}
