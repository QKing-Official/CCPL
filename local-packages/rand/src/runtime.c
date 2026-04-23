// rand package runtime
// This is very basic! Dont trust the randomisation fully!
// This isnt really random!
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

static int _ccpl_rand_seeded = 0;
static uint64_t _ccpl_rand_state[4] = {
    0x243f6a8885a308d3ULL,
    0x13198a2e03707344ULL,
    0xa4093822299f31d0ULL,
    0x082efa98ec4e6c89ULL
};

static uint64_t _ccpl_rand_rotl(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

static uint64_t _ccpl_rand_splitmix64(uint64_t *x) {
    uint64_t z;
    *x += 0x9e3779b97f4a7c15ULL;
    z = *x;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static uint64_t _ccpl_rand_next_u64(void) {
    uint64_t result = _ccpl_rand_rotl(_ccpl_rand_state[1] * 5ULL, 7) * 9ULL;
    uint64_t t = _ccpl_rand_state[1] << 17;

    _ccpl_rand_state[2] ^= _ccpl_rand_state[0];
    _ccpl_rand_state[3] ^= _ccpl_rand_state[1];
    _ccpl_rand_state[1] ^= _ccpl_rand_state[2];
    _ccpl_rand_state[0] ^= _ccpl_rand_state[3];

    _ccpl_rand_state[2] ^= t;
    _ccpl_rand_state[3] = _ccpl_rand_rotl(_ccpl_rand_state[3], 45);

    return result;
}

static uint32_t _ccpl_rand_next_u32(void) {
    return (uint32_t)(_ccpl_rand_next_u64() >> 32);
}

static void _ccpl_rand_seed_u64(uint64_t seed) {
    uint64_t x = seed;
    _ccpl_rand_state[0] = _ccpl_rand_splitmix64(&x);
    _ccpl_rand_state[1] = _ccpl_rand_splitmix64(&x);
    _ccpl_rand_state[2] = _ccpl_rand_splitmix64(&x);
    _ccpl_rand_state[3] = _ccpl_rand_splitmix64(&x);

    if ((_ccpl_rand_state[0] | _ccpl_rand_state[1] | _ccpl_rand_state[2] | _ccpl_rand_state[3]) == 0ULL) {
        _ccpl_rand_state[0] = 1ULL;
    }
    _ccpl_rand_seeded = 1;
}

static uint64_t _ccpl_rand_entropy_seed(void) {
    uint64_t seed = 0;
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        uint64_t tmp[2] = {0, 0};
        ssize_t n = read(fd, tmp, sizeof(tmp));
        close(fd);
        if (n == (ssize_t)sizeof(tmp)) {
            seed = tmp[0] ^ (tmp[1] + 0x9e3779b97f4a7c15ULL);
            return seed;
        }
    }

    {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        seed ^= ((uint64_t)ts.tv_sec << 32) ^ (uint64_t)ts.tv_nsec;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        seed ^= (((uint64_t)ts.tv_sec << 32) ^ (uint64_t)ts.tv_nsec) * 0xbf58476d1ce4e5b9ULL;
    }
    seed ^= (uint64_t)getpid() * 0x94d049bb133111ebULL;
    seed ^= (uint64_t)(uintptr_t)&seed;
    return seed;
}

static void ccpl_rand_seed(double seed) {
    uint64_t s = (uint64_t)(seed < 0.0 ? -seed : seed);
    s ^= (uint64_t)(seed * 1000003.0);
    _ccpl_rand_seed_u64(s ? s : 1ULL);
}

static void _ccpl_rand_ensure_seed(void) {
    if (!_ccpl_rand_seeded) {
        _ccpl_rand_seed_u64(_ccpl_rand_entropy_seed());
    }
}

static uint32_t _ccpl_rand_bounded_u32(uint32_t bound) {
    uint32_t x;
    uint32_t thresh;
    if (bound == 0U) return 0U;
    thresh = (uint32_t)(-bound) % bound;
    for (;;) {
        x = _ccpl_rand_next_u32();
        if (x >= thresh) return x % bound;
    }
}

static int ccpl_rand_int(int min, int max) {
    uint32_t span;
    _ccpl_rand_ensure_seed();
    if (max < min) {
        int t = min;
        min = max;
        max = t;
    }
    span = (uint32_t)((uint64_t)(max - min) + 1ULL);
    return min + (int)_ccpl_rand_bounded_u32(span);
}

static double ccpl_rand_float(void) {
    _ccpl_rand_ensure_seed();
    return (_ccpl_rand_next_u64() >> 11) * (1.0 / 9007199254740992.0);
}

static int ccpl_rand_choice_idx(int n) {
    _ccpl_rand_ensure_seed();
    if (n <= 0) return -1;
    return (int)_ccpl_rand_bounded_u32((uint32_t)n);
}
