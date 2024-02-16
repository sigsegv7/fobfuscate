/*
 * Copyright (c) 2023-2024 Ian Marco Moffett and the Osmora team.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Osmora nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include <info.h>
#if defined(__x86_64__)
#include <sse_accel.h>
#endif  /* defined(__x86_64__) */

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Big endian machines not supported yet"
#endif

#if defined(__x86_64__)
#define cpuid(level, a, b, c, d)					\
  __asm__ __volatile__ ("cpuid\n\t"					\
			: "=a" (a), "=b" (b), "=c" (c), "=d" (d)	\
			: "0" (level))
#endif

#define flip_block(TMP_VAR, TYPE, BUF, POS)         \
        TMP_VAR = *(TYPE *)&BUF[POS];               \
        TMP_VAR = ~TMP_VAR;                         \
        *(TYPE *)&BUF[POS] = TMP_VAR;               \

static char *
read_file(const char *fname, size_t *size_out)
{
    FILE *fp;
    char *buf;
    size_t bufsize;

    if (access(fname, F_OK) != 0) {
        fprintf(stderr, "%s does not exist!\n");
        return NULL;
    }

    fp = fopen(fname, "r");

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    bufsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    buf = malloc(bufsize);
    fread(buf, sizeof(char), bufsize, fp);
    fclose(fp);

    *size_out = bufsize;

    return buf;
}

static void
writeback_file(const char *fname, const char *buf, size_t buf_size)
{
    FILE *fp;

    fp = fopen(fname, "w");
    fwrite(buf, buf_size, sizeof(char), fp);
    fclose(fp);
}

#if defined(__x86_64__)
static inline bool
is_sse3_supported(void)
{
    uint32_t ecx, unused;

    cpuid(0x0000001, unused, unused, ecx, unused);
    return (ecx & (1 << 0)) != 0;
}

static inline bool
is_sse2_supported(void)
{
    uint32_t edx, unused;
    cpuid(0x0000001, unused, unused, unused, edx);
    return (edx & (1 << 26)) != 0;
}

static void
amd64_cpu_tests(struct cpu_info *info)
{
    bool sse3_supported;

    sse3_supported = false;

    if (is_sse3_supported()) {
        printf("[?]: SSE3 supported, may use as optimization\n");
        sse3_supported = true;
        info->has_sse3 = 1;
    }

    if (!sse3_supported) {
        if (is_sse2_supported()) {
            printf("[?]: SSE2 supported, may use as optimization\n");
            info->has_sse2 = 1;
        }
    }
}
#endif  /* defined(__x86_64__) */

static char *
encrypt(const struct cpu_info *info, char *buf, size_t buf_size)
{
    size_t current_pos;
    size_t step;
    uint64_t tmp;

    current_pos = 0;
    step = 8;           /* Start at 8 bytes (64 bits) */

#if defined(__x86_64__)
    if (info->has_sse2 || info->has_sse3) {
        step <<= 1;         /* Start at 16 bytes (128 bits) */
    }
#endif  /* defined(__x86_64__) */

    while (current_pos < buf_size) {
        /* Ensure we aren't over 16 bytes and a power of two */
        if (step != 1) {
            assert((step & 1) == 0 && step <= 16);
        }

        /* Ensure we don't cause any overflows */
        while (((current_pos + step) >= buf_size) && step > 1)
            /* Essentially divide the step by 2, just faster */
            step >>= 1;

        switch (step) {
        case 16:
            accel_invert128((uintptr_t)buf + current_pos);
            break;
        case 8:
            flip_block(tmp, uint64_t, buf, current_pos);
            break;
        case 4:
            flip_block(tmp, uint32_t, buf, current_pos);
            break;
        case 2:
            flip_block(tmp, uint16_t, buf, current_pos);
            break;
        case 1:
            flip_block(tmp, uint8_t, buf, current_pos);
            break;
        }

        current_pos += step;
    }
}

int
main(int argc, const char **argv)
{
    size_t buf_size;
    char *buf;
    struct cpu_info info = { 0 };

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

#if defined(__x86_64__)
    amd64_cpu_tests(&info);
#endif  /* __x86_64__ */

    buf = read_file(argv[1], &buf_size);
    encrypt(&info, buf, buf_size);
    writeback_file(argv[1], buf, buf_size);
    free(buf);

    return 0;
}
