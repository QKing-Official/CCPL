// Standalone CCPL crypto package runtime
// It is really simple, maybe I need to add more here
#include <stdint.h>

static char _ccpl_crypto_buf1[64];
static char _ccpl_crypto_buf2[64];
static char _ccpl_crypto_buf3[4096];

static const char *ccpl_crypto_fnv1a64(const char *s) {
    uint64_t h = 1469598103934665603ULL;
    if (s) {
        while (*s) {
            h ^= (unsigned char)(*s++);
            h *= 1099511628211ULL;
        }
    }
    snprintf(_ccpl_crypto_buf1, sizeof(_ccpl_crypto_buf1), "%016llx", (unsigned long long)h);
    return _ccpl_crypto_buf1;
}

static const char *ccpl_crypto_crc32(const char *s) {
    uint32_t crc = 0xFFFFFFFFu;
    if (s) {
        while (*s) {
            crc ^= (unsigned char)(*s++);
            for (int i = 0; i < 8; i++) {
                uint32_t mask = (uint32_t)(-(int)(crc & 1u));
                crc = (crc >> 1) ^ (0xEDB88320u & mask);
            }
        }
    }
    crc ^= 0xFFFFFFFFu;
    snprintf(_ccpl_crypto_buf2, sizeof(_ccpl_crypto_buf2), "%08x", crc);
    return _ccpl_crypto_buf2;
}

static const char *ccpl_crypto_rot13(const char *s) {
    if (!s) return "";
    size_t n = strlen(s);
    if (n > sizeof(_ccpl_crypto_buf3) - 1) n = sizeof(_ccpl_crypto_buf3) - 1;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = (char)('a' + ((c - 'a' + 13) % 26));
        else if (c >= 'A' && c <= 'Z') c = (char)('A' + ((c - 'A' + 13) % 26));
        _ccpl_crypto_buf3[i] = c;
    }
    _ccpl_crypto_buf3[n] = '\0';
    return _ccpl_crypto_buf3;
}
