// code from https://gist.githubusercontent.com/noonat/1127614/raw/f4d0c4612653aaf7a3d16aa4e21d9f36f416487c/extract_pak.c
// Extract a PAK file (from Quake 1 and 2)

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    char id[4];
    int dir_offset;
    int dir_length;
} pak_header_t;

typedef struct {
    char name[56];
    int offset;
    int length;
} pak_entry_t;

char *dirname(const char *path) {
    const char *p = strrchr(path, '/');
    if (p) {
        size_t n = p - path;
        if (n > 0) {
            char *dir = malloc(n + 1);
            strncpy(dir, path, n);
            dir[n] = 0;
            return dir;
        }
    }
    return NULL;
}

int mkdir_p(const char *dir, mode_t mode) {
    int rv = mkdir(dir, mode);
    if (rv < 0 && errno == ENOENT) {
        char *parent_dir = dirname(dir);
        if (parent_dir) {
            mkdir_p(parent_dir, mode);
            free(parent_dir);
            rv = mkdir(dir, mode);
        }
    }
    return rv;
}

int main(const int argc, const char **argv) {
    int i;
    char buf[4096];

    if (argc < 1) {
        fprintf(stderr, "usage: %s <pak file>\n", argv[0]);
        return 1;
    }

    fprintf(stderr, "opening file %s\n", argv[1]);
    FILE *pak = (FILE *)fopen(argv[1], "rb");
    if (!pak) {
        fprintf(stderr, "error opening %s for reading\n", argv[1]);
        return 1;
    } else {
        fprintf(stderr, "file ok\n");
    }

    pak_header_t header;
    fread(&header, sizeof(pak_header_t), 1, pak);
    fprintf(stderr, "read header ok\n");

    if (strncmp(header.id, "PACK", 4) == 0) {
        fprintf(stderr, "header id ok\n");
    } else {
        fprintf(stderr, "invalid header id\n");
        fclose(pak);
        return 1;
    }

    if (header.dir_length % sizeof(pak_entry_t) == 0) {
        fprintf(stderr, "dir length ok\n");
    } else {
        fprintf(stderr, "invalid dir length\n");
        fclose(pak);
        return 1;
    }

    pak_entry_t *entries = malloc(header.dir_length);
    if (!entries) {
        fprintf(stderr, "error allocating entries array\n");
        fclose(pak);
        return 1;
    }

    fseek(pak, header.dir_offset, SEEK_SET);
    fread(entries, header.dir_length, 1, pak);

    pak_entry_t *entry = entries;
    int num_entries = header.dir_length / sizeof(pak_entry_t);
    for (i = 0; i < num_entries; ++i, ++entry) {
        fprintf(stderr, "%d: %s (%d, %d)\n",
            i, entry->name, entry->offset, entry->length);

        char *dir = dirname(entry->name);
        if (dir) {
            mkdir_p(dir, 0777);
            free(dir);
        }

        FILE *out = fopen(entry->name, "wb");
        if (!out) {
            fprintf(stderr, "error opening %s for writing\n", entry->name);
            continue;
        }

        char *buf = malloc(entry->length);
        if (!buf) {
            fprintf(stderr, "error allocating buffer for entry\n");
            continue;
        }

        fseek(pak, entry->offset, SEEK_SET);
        if (fread(buf, entry->length, 1, pak) < 0) {
            fprintf(stderr, "error reading entry\n");
        } else if (fwrite(buf, entry->length, 1, out) < 0) {
            fprintf(stderr, "error writing entry\n");
        }

        free(buf);

        fclose(out);
    }

    free(entries);
    fclose(pak);
    return 0;
}