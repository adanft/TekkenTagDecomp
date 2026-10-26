/*
 * ttt-sign: makes the signing key of update manifests and signs them, offline (T46, docs/port/matchmaking.md).
 * The game only ever verifies (port/src/plat_net.c, with the public key in its net.update_key setting); this tool
 * holds the secret key and should run on a machine the server does not share.
 *
 *   ttt-sign keygen <secret-key-file>          a new key: the secret seed (64 hex digits) into the file (mode 0600),
 *                                              the public key (64 hex digits) on stdout
 *   ttt-sign public <secret-key-file>          the public key again
 *   ttt-sign sign <secret-key-file> <in> <out> the manifest <in> with a "signature <128 hex digits>" line after it
 *   ttt-sign verify <public-key-hex> <file>    exit 0 when the file's signature line is the key's signature
 *
 * A manifest is text: "ttt-update 1", then "version <text>", "build <16 hex digits>" (the build id of the newest
 * build, as the game prints it: "ttt: update: this build is <id>" with TTT_UPDATE_CHECK=1), optional "url <text>"
 * and "notes <text>" lines. The signature covers every byte before the signature line.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../third_party/tweetnacl/ed25519.h"

#define MANIFEST_MAX 1100

static int unhex(const char *s, uint8_t *out, int n) {
    int i;
    for (i = 0; i < 2 * n; i++) {
        char c = s[i];
        int v = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        if (v < 0) {
            return -1;
        }
        out[i / 2] = (uint8_t)(i % 2 ? out[i / 2] | v : v << 4);
    }
    return s[2 * n] == 0 || s[2 * n] == '\n' || s[2 * n] == '\r' ? 0 : -1;
}

static void hex(const uint8_t *p, int n, char *out) {
    int i;
    for (i = 0; i < n; i++) {
        snprintf(out + 2 * i, 3, "%02x", p[i]);
    }
}

static int read_file(const char *path, uint8_t *buf, size_t max, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "ttt-sign: %s: %s\n", path, strerror(errno));
        return -1;
    }
    *n = fread(buf, 1, max, f);
    if (fgetc(f) != EOF) {
        fprintf(stderr, "ttt-sign: %s: larger than %u bytes\n", path, (unsigned)max);
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

static int read_seed(const char *path, uint8_t seed[32]) {
    uint8_t buf[200];
    size_t n;
    if (read_file(path, buf, sizeof buf - 1, &n) != 0) {
        return -1;
    }
    buf[n] = 0;
    if (unhex((const char *)buf, seed, 32) != 0) {
        fprintf(stderr, "ttt-sign: %s: not a secret key (64 hex digits)\n", path);
        return -1;
    }
    return 0;
}

/* The signed part: everything before a "signature " line at the start of a line (or the whole file). */
static size_t signed_len(const uint8_t *m, size_t n, const char **sigline) {
    size_t i;
    for (i = 0; i + 10 <= n; i++) {
        if ((i == 0 || m[i - 1] == '\n') && memcmp(m + i, "signature ", 10) == 0) {
            *sigline = (const char *)m + i + 10;
            return i;
        }
    }
    *sigline = NULL;
    return n;
}

int main(int argc, char **argv) {
    uint8_t seed[32], pk[32], sig[64], m[MANIFEST_MAX + 1];
    char text[200];
    size_t n;
    const char *line;
    if (argc == 3 && strcmp(argv[1], "keygen") == 0) {
        int fd = open("/dev/urandom", O_RDONLY), out;
        if (fd < 0 || read(fd, seed, 32) != 32) {
            fprintf(stderr, "ttt-sign: no randomness (/dev/urandom)\n");
            return 1;
        }
        close(fd);
        out = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (out < 0) {
            fprintf(stderr, "ttt-sign: %s: %s (an existing key is never overwritten)\n", argv[2], strerror(errno));
            return 1;
        }
        hex(seed, 32, text);
        text[64] = '\n';
        if (write(out, text, 65) != 65 || close(out) != 0) {
            fprintf(stderr, "ttt-sign: %s: cannot write\n", argv[2]);
            return 1;
        }
        ed25519_public_key(pk, seed);
        hex(pk, 32, text);
        printf("%s\n", text);
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "public") == 0) {
        if (read_seed(argv[2], seed) != 0) {
            return 1;
        }
        ed25519_public_key(pk, seed);
        hex(pk, 32, text);
        printf("%s\n", text);
        return 0;
    }
    if (argc == 5 && strcmp(argv[1], "sign") == 0) {
        FILE *f;
        if (read_seed(argv[2], seed) != 0 || read_file(argv[3], m, MANIFEST_MAX - 140, &n) != 0) {
            return 1;
        }
        n = signed_len(m, n, &line); /* (signing again replaces the old signature) */
        if (n == 0 || m[n - 1] != '\n') {
            fprintf(stderr, "ttt-sign: %s: empty, or its last line has no newline\n", argv[3]);
            return 1;
        }
        ed25519_sign(sig, m, n, seed);
        hex(sig, 64, text);
        f = fopen(argv[4], "wb");
        if (f == NULL || fwrite(m, 1, n, f) != n || fprintf(f, "signature %s\n", text) < 0 || fclose(f) != 0) {
            fprintf(stderr, "ttt-sign: %s: cannot write\n", argv[4]);
            return 1;
        }
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "verify") == 0) {
        if (unhex(argv[2], pk, 32) != 0) {
            fprintf(stderr, "ttt-sign: not a public key (64 hex digits)\n");
            return 2;
        }
        if (read_file(argv[3], m, MANIFEST_MAX, &n) != 0) {
            return 2;
        }
        m[n] = 0;
        n = signed_len(m, n, &line);
        if (line == NULL || unhex(line, sig, 64) != 0 || ed25519_verify(sig, m, n, pk) != 0) {
            printf("ttt-sign: %s: BAD signature\n", argv[3]);
            return 1;
        }
        printf("ttt-sign: %s: good signature\n", argv[3]);
        return 0;
    }
    fprintf(stderr, "usage: ttt-sign keygen <secret-key-file> | public <secret-key-file> |\n"
                    "                sign <secret-key-file> <manifest> <signed-manifest> | verify <public-hex> <file>\n");
    return 2;
}
