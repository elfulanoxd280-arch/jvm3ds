#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>

// ─── ZIP/JAR structs ─────────────────────────────────────────────────────────
typedef struct {
    uint32_t signature;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;
    uint16_t mod_time, mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len;
    uint16_t extra_len;
} __attribute__((packed)) LocalFileHeader;

typedef struct {
    uint32_t signature;
    uint16_t version_made, version_needed;
    uint16_t flags, compression;
    uint16_t mod_time, mod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_len, extra_len, comment_len;
    uint16_t disk_start;
    uint16_t internal_attr;
    uint32_t external_attr;
    uint32_t local_offset;
} __attribute__((packed)) CDEntry;

typedef struct {
    uint32_t signature;
    uint16_t disk_num, start_disk;
    uint16_t disk_entries, total_entries;
    uint32_t cd_size, cd_offset;
    uint16_t comment_len;
} __attribute__((packed)) EOCDRecord;

// ─── Extraer archivo de JAR a memoria ────────────────────────────────────────
uint8_t* jar_extract(FILE *f, const char *filename, uint32_t *out_size) {
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);

    EOCDRecord eocd;
    int found = 0;
    for (long pos = fsize - sizeof(EOCDRecord); pos >= 0; pos--) {
        fseek(f, pos, SEEK_SET);
        fread(&eocd, 1, sizeof(EOCDRecord), f);
        if (eocd.signature == 0x06054b50) { found = 1; break; }
    }
    if (!found) return NULL;

    fseek(f, eocd.cd_offset, SEEK_SET);
    uint32_t local_off = 0;
    uint16_t compression = 0;
    uint32_t comp_size = 0, uncomp_size = 0;
    int file_found = 0;

    for (int i = 0; i < eocd.total_entries; i++) {
        CDEntry cd;
        fread(&cd, 1, sizeof(CDEntry), f);
        if (cd.signature != 0x02014b50) break;

        char name[256] = {0};
        int nl = cd.filename_len < 255 ? cd.filename_len : 255;
        fread(name, 1, nl, f);
        fseek(f, cd.extra_len + cd.comment_len, SEEK_CUR);

        if (strcmp(name, filename) == 0) {
            local_off   = cd.local_offset;
            compression = cd.compression;
            comp_size   = cd.compressed_size;
            uncomp_size = cd.uncompressed_size;
            file_found  = 1;
            break;
        }
    }
    if (!file_found) return NULL;

    fseek(f, local_off, SEEK_SET);
    LocalFileHeader lfh;
    fread(&lfh, 1, sizeof(LocalFileHeader), f);
    fseek(f, lfh.filename_len + lfh.extra_len, SEEK_CUR);

    uint8_t *comp_data = malloc(comp_size);
    fread(comp_data, 1, comp_size, f);

    uint8_t *out = malloc(uncomp_size);
    *out_size = uncomp_size;

    if (compression == 0) {
        memcpy(out, comp_data, uncomp_size);
    } else {
        z_stream zs = {0};
        zs.next_in   = comp_data;
        zs.avail_in  = comp_size;
        zs.next_out  = out;
        zs.avail_out = uncomp_size;
        inflateInit2(&zs, -MAX_WBITS);
        inflate(&zs, Z_FINISH);
        inflateEnd(&zs);
    }
    free(comp_data);
    return out;
}

// Listar clases en un JAR
// Retorna lista de nombres, *count = cantidad
char** jar_list_classes(FILE *f, int *count) {
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);

    EOCDRecord eocd;
    for (long pos = fsize - sizeof(EOCDRecord); pos >= 0; pos--) {
        fseek(f, pos, SEEK_SET);
        fread(&eocd, 1, sizeof(EOCDRecord), f);
        if (eocd.signature == 0x06054b50) break;
    }

    char **names = malloc(eocd.total_entries * sizeof(char*));
    *count = 0;

    fseek(f, eocd.cd_offset, SEEK_SET);
    for (int i = 0; i < eocd.total_entries; i++) {
        CDEntry cd;
        fread(&cd, 1, sizeof(CDEntry), f);
        if (cd.signature != 0x02014b50) break;

        char name[256] = {0};
        int nl = cd.filename_len < 255 ? cd.filename_len : 255;
        fread(name, 1, nl, f);
        fseek(f, cd.extra_len + cd.comment_len, SEEK_CUR);

        // Solo archivos .class
        int len = strlen(name);
        if (len > 6 && strcmp(name + len - 6, ".class") == 0) {
            names[*count] = strdup(name);
            (*count)++;
        }
    }
    return names;
}
