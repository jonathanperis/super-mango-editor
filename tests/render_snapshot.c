/*
 * render_snapshot.c — See render_snapshot.h for what a snapshot is.
 *
 * Three small pieces: a pixel hash (FNV-1a, 64 bit, over the width, the
 * height and the RGBA bytes), the text file of recorded hashes, and a PNG
 * writer. raylib is built without image export (tools/build_raylib.py), so
 * the PNG is written here, with "stored" (uncompressed) deflate blocks: any
 * viewer opens it, and `make render-snapshots-update` recompresses the
 * copies it commits (tools/render_snapshots.py).
 */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "render_snapshot.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>    /* _mkdir */
#else
#include <sys/stat.h>  /* mkdir */
#endif

#include "test_paths.h"

#define SNAPSHOT_DIR   "tests/snapshots/"
#define SNAPSHOT_LIST  SNAPSHOT_DIR "render_snapshots.txt"
#define SNAPSHOT_LINES 64

Image snapshot_grab_screen(void)
{
    Image image = LoadImageFromScreen();
    if (!IsImageValid(image)) return image;
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
#ifdef MANGO_MEMORY_TESTS
    /* raylib 6.2's Memory backend: bottom row first, blue and red swapped. */
    ImageFlipVertical(&image);
    unsigned char *p = image.data;
    for (int i = 0; i < image.width * image.height; i++, p += 4) {
        unsigned char red = p[0];
        p[0] = p[2];
        p[2] = red;
    }
#endif
    return image;
}

/* Everything below compares or records frames, which only the Memory
 * backend does (see render_snapshot.h); a desktop build leaves it out. */
#ifdef MANGO_MEMORY_TESTS

static uint64_t pixel_hash(Image image)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    const uint32_t size[2] = {(uint32_t)image.width, (uint32_t)image.height};
    const unsigned char *bytes[2] = {(const unsigned char *)size, image.data};
    const size_t lengths[2] = {sizeof(size), (size_t)image.width * (size_t)image.height * 4};
    for (int part = 0; part < 2; part++)
        for (size_t i = 0; i < lengths[part]; i++) {
            hash ^= bytes[part][i];
            hash *= UINT64_C(1099511628211);
        }
    return hash;
}

/* ---- PNG writer ---------------------------------------------------- */

static uint32_t crc_table[256];

static uint32_t crc32_update(uint32_t crc, const unsigned char *data, size_t size)
{
    if (!crc_table[1])
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_table[n] = c;
        }
    for (size_t i = 0; i < size; i++) crc = crc_table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    return crc;
}

static void put_u32(unsigned char *out, uint32_t value)
{
    out[0] = (unsigned char)(value >> 24);
    out[1] = (unsigned char)(value >> 16);
    out[2] = (unsigned char)(value >> 8);
    out[3] = (unsigned char)value;
}

/* One chunk: length, type, data, CRC of type and data. */
static int write_chunk(FILE *fp, const char *type, const unsigned char *data, size_t size)
{
    unsigned char head[8], tail[4];
    put_u32(head, (uint32_t)size);
    memcpy(head + 4, type, 4);
    uint32_t crc = crc32_update(0xffffffffu, head + 4, 4);
    crc = crc32_update(crc, data, size) ^ 0xffffffffu;
    put_u32(tail, crc);
    return fwrite(head, 1, 8, fp) == 8 && (size == 0 || fwrite(data, 1, size, fp) == size) &&
           fwrite(tail, 1, 4, fp) == 4 ? 0 : -1;
}

/* An RGBA image as a PNG: each row is filter byte 0 plus the pixels, and
 * the zlib stream holds them in stored blocks of at most 65535 bytes. */
static int write_png(const char *path, Image image)
{
    const size_t row = (size_t)image.width * 4 + 1;
    const size_t raw_size = row * (size_t)image.height;
    const size_t blocks = raw_size / 65535 + 1;
    const size_t zsize = 2 + raw_size + blocks * 5 + 4;
    unsigned char *raw = malloc(raw_size), *z = malloc(zsize);
    unsigned char header[13];
    FILE *fp = NULL;
    int result = -1;

    if (!raw || !z) goto done;
    for (int y = 0; y < image.height; y++) {
        raw[(size_t)y * row] = 0;
        memcpy(raw + (size_t)y * row + 1,
               (const unsigned char *)image.data + (size_t)y * image.width * 4, row - 1);
    }
    size_t at = 0, left = raw_size;
    uint32_t a = 1, b = 0;  /* Adler-32 of the uncompressed bytes */
    z[at++] = 0x78;
    z[at++] = 0x01;
    for (size_t offset = 0; left > 0; ) {
        size_t take = left < 65535 ? left : 65535;
        z[at++] = (unsigned char)(take == left);  /* BFINAL on the last block */
        z[at++] = (unsigned char)take;
        z[at++] = (unsigned char)(take >> 8);
        z[at++] = (unsigned char)~take;
        z[at++] = (unsigned char)(~take >> 8);
        memcpy(z + at, raw + offset, take);
        for (size_t i = 0; i < take; i++) {
            a = (a + raw[offset + i]) % 65521u;
            b = (b + a) % 65521u;
        }
        at += take;
        offset += take;
        left -= take;
    }
    put_u32(z + at, (b << 16) | a);
    at += 4;

    put_u32(header, (uint32_t)image.width);
    put_u32(header + 4, (uint32_t)image.height);
    header[8] = 8;   /* bits per channel */
    header[9] = 6;   /* RGBA */
    header[10] = header[11] = header[12] = 0;
    fp = fopen(path, "wb");
    if (!fp) goto done;
    if (fwrite("\x89PNG\r\n\x1a\n", 1, 8, fp) != 8 ||
        write_chunk(fp, "IHDR", header, sizeof(header)) ||
        write_chunk(fp, "IDAT", z, at) ||
        write_chunk(fp, "IEND", NULL, 0)) goto done;
    result = 0;
done:
    if (fp && fclose(fp) != 0) result = -1;
    free(raw);
    free(z);
    return result;
}

/* ---- The list of recorded hashes ------------------------------------ */

typedef struct {
    char name[64];
    int width, height;
    unsigned long long hash;
} SnapshotLine;

static int read_list(SnapshotLine *lines, int capacity)
{
    FILE *fp = fopen(SNAPSHOT_LIST, "r");
    char text[256];
    int count = 0;
    if (!fp) return 0;
    while (count < capacity && fgets(text, sizeof(text), fp)) {
        SnapshotLine *line = &lines[count];
        if (text[0] == '#' || text[0] == '\n') continue;
        if (sscanf(text, "%63s %d %d %llx", line->name, &line->width, &line->height,
                   &line->hash) == 4) count++;
    }
    fclose(fp);
    return count;
}

static int write_list(const SnapshotLine *lines, int count)
{
    FILE *fp = fopen(SNAPSHOT_LIST, "w");
    if (!fp) return -1;
    fputs("# Render snapshots (raylib Memory backend): name, width, height, FNV-1a 64\n"
          "# hash of the RGBA pixels. The frames are the PNGs beside this file.\n"
          "# Written by `make render-snapshots-update`; see tests/render_snapshot.h.\n", fp);
    for (int i = 0; i < count; i++)
        fprintf(fp, "%s %d %d %016llx\n", lines[i].name, lines[i].width, lines[i].height,
                lines[i].hash);
    return fclose(fp);
}

static void make_folder(const char *path)
{
#ifdef _WIN32
    (void)_mkdir(path);
#else
    (void)mkdir(path, 0755);
#endif
}

/* Count the pixels that differ from the recorded PNG, and find the first. */
static void describe_difference(const char *name, Image image)
{
    char path[256];
    snprintf(path, sizeof(path), SNAPSHOT_DIR "%s.png", name);
    Image recorded = LoadImage(path);
    if (!IsImageValid(recorded)) {
        fprintf(stderr, "snapshot %s: no recorded frame at %s to compare with\n", name, path);
        return;
    }
    ImageFormat(&recorded, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (recorded.width != image.width || recorded.height != image.height) {
        fprintf(stderr, "snapshot %s: size %dx%d, recorded %dx%d\n", name,
                image.width, image.height, recorded.width, recorded.height);
    } else {
        const unsigned char *now = image.data, *then = recorded.data;
        long differ = 0;
        int first = -1, worst = 0;
        for (int i = 0; i < image.width * image.height; i++) {
            int delta = 0;
            for (int c = 0; c < 4; c++) {
                int d = abs(now[i * 4 + c] - then[i * 4 + c]);
                if (d > delta) delta = d;
            }
            if (!delta) continue;
            if (first < 0) first = i;
            if (delta > worst) worst = delta;
            differ++;
        }
        if (differ == 0)
            fprintf(stderr, "snapshot %s: the pixels match %s; only its line in "
                    SNAPSHOT_LIST " is out of date\n", name, path);
        else
            fprintf(stderr, "snapshot %s: %ld of %d pixels differ from %s (largest channel "
                    "change %d), the first at x=%d y=%d\n", name, differ,
                    image.width * image.height, path, worst, first % image.width,
                    first / image.width);
    }
    UnloadImage(recorded);
}

#endif /* MANGO_MEMORY_TESTS */

int snapshot_check(const char *name, Image image)
{
#ifndef MANGO_MEMORY_TESTS
    /* A GPU may rasterise an edge differently: only the software renderer's
     * frames are recorded (see render_snapshot.h). */
    printf("snapshot %s: not compared on this backend (Memory backend only)\n", name);
    (void)image;
    return 0;
#else
    static SnapshotLine lines[SNAPSHOT_LINES];
    const char *update = getenv("MANGO_SNAPSHOT_UPDATE");
    char actual[256];
    int count, index = -1;

    if (!IsImageValid(image) || image.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        fprintf(stderr, "snapshot %s: no frame was read back\n", name);
        return 1;
    }
    const uint64_t hash = pixel_hash(image);
    count = read_list(lines, SNAPSHOT_LINES);
    for (int i = 0; i < count; i++)
        if (strcmp(lines[i].name, name) == 0) index = i;

    make_folder(MANGO_TEST_OUTDIR);
    make_folder(TEST_OUT "snapshots");
    snprintf(actual, sizeof(actual), TEST_OUT "snapshots/%s.png", name);

    if (update && strcmp(update, "1") == 0) {
        if (index < 0) {
            if (count == SNAPSHOT_LINES) return 1;
            index = count++;
        }
        snprintf(lines[index].name, sizeof(lines[index].name), "%s", name);
        lines[index].width = image.width;
        lines[index].height = image.height;
        lines[index].hash = (unsigned long long)hash;
        /* Keep the file sorted, so a re-record changes only its own line. */
        for (int i = 1; i < count; i++)
            for (int j = i; j > 0 && strcmp(lines[j - 1].name, lines[j].name) > 0; j--) {
                SnapshotLine swap = lines[j];
                lines[j] = lines[j - 1];
                lines[j - 1] = swap;
            }
        if (write_list(lines, count) != 0 || write_png(actual, image) != 0) {
            fprintf(stderr, "snapshot %s: could not record it\n", name);
            return 1;
        }
        printf("snapshot %s: recorded %016llx (%s)\n", name, (unsigned long long)hash, actual);
        return 0;
    }

    if (index >= 0 && lines[index].hash == (unsigned long long)hash &&
        lines[index].width == image.width && lines[index].height == image.height)
        return 0;

    if (index < 0)
        fprintf(stderr, "snapshot %s: not recorded in %s\n", name, SNAPSHOT_LIST);
    else
        fprintf(stderr, "snapshot %s: CHANGED: hash %016llx, recorded %016llx\n", name,
                (unsigned long long)hash, lines[index].hash);
    describe_difference(name, image);
    if (write_png(actual, image) == 0)
        fprintf(stderr, "snapshot %s: the new frame is %s\n", name, actual);
    fprintf(stderr, "snapshot %s: if the change is intended, run `make render-snapshots-update` "
            "and commit tests/snapshots/\n", name);
    return 1;
#endif
}
