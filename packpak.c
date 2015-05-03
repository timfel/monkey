#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>


#define WRITE_DDS

#define DWORD uint32_t

#define DDSD_CAPS                       0x00000001
#define DDSD_HEIGHT                     0x00000002
#define DDSD_WIDTH                      0x00000004
#define DDSD_PITCH                      0x00000008
#define DDSD_PIXELFORMAT                0x00001000
#define DDSD_MIPMAPCOUNT                0x00020000
#define DDSD_LINEARSIZE                 0x00080000
#define DDSD_DEPTH                      0x00800000
#define DDPF_ALPHAPIXELS                0x00000001
#define DDPF_FOURCC                     0x00000004
#define DDPF_RGB                        0x00000040
#define DDSCAPS_COMPLEX                 0x00000008
#define DDSCAPS_TEXTURE                 0x00001000
#define DDSCAPS_MIPMAP                  0x00400000
#define DDSCAPS2_CUBEMAP                0x00000200
#define DDSCAPS2_CUBEMAP_POSITIVEX      0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX      0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY      0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY      0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ      0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ      0x00008000
#define DDSCAPS2_VOLUME                 0x00200000

typedef struct {
    DWORD dwMagic;
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwHeight;
    DWORD dwWidth;
    DWORD dwPitchOrLinearSize;
    DWORD dwDepth;
    DWORD dwMipMapCount;
    DWORD dwReserved1[11];
    /*  DDPIXELFORMAT   */
    struct {
        DWORD dwSize;
        DWORD dwFlags;
        DWORD dwFourCC;
        DWORD dwRGBBitCount;
        DWORD dwRBitMask;
        DWORD dwGBitMask;
        DWORD dwBBitMask;
        DWORD dwAlphaBitMask;
    } sPixelFormat;
    /*  DDCAPS2 */
    struct {
        DWORD dwCaps1;
        DWORD dwCaps2;
        DWORD dwDDSX;
        DWORD dwReserved;
    } sCaps;
    DWORD dwReserved2;
} DDS_header;


typedef struct PakHeader {
    DWORD magic;                /* KAPL -> "LPAK" */
    float version;
    DWORD startOfIndex;         /* -> 1 DWORD per file */
    DWORD startOfFileEntries;   /* -> 5 DWORD per file */
    DWORD startOfFileNames;     /* zero-terminated string */
    DWORD startOfData;
    DWORD sizeOfIndex;
    DWORD sizeOfFileEntries;
    DWORD sizeOfFileNames;
    DWORD sizeOfData;
} PakHeader;

typedef struct PakFileEntry {
    DWORD fileDataPos;          /* + startOfData */
    DWORD fileNamePos;          /* + startOfFileNames */
    DWORD dataSize;
    DWORD dataSize2;            /* real size? (always =dataSize) */
    DWORD compressed;           /* compressed? (always 0) */
} PakFileEntry;

/* Recursive mkdir from http://nion.modprobe.de/blog/archives/357-Recursive-directory-creation.html */
static void _mkdir(const char *path)
{
    char opath[256];
    char *p;
    size_t len;

    strncpy(opath, path, sizeof(opath));
    len = strlen(opath);
    if (opath[len - 1] == '/')
        opath[len - 1] = '\0';
    for (p = opath; *p; p++)
        if (*p == '/') {
            *p = '\0';
            if (access(opath, F_OK))
                mkdir(opath, S_IRWXU);
            *p = '/';
        }
    if (access(opath, F_OK))    /* if path is not terminated with / */
        mkdir(opath, S_IRWXU);
}

int main(int argc, char **argv)
{
    PakHeader header;
    PakFileEntry *entries;
    char fileName[FILENAME_MAX];
    char dirName[FILENAME_MAX];
    unsigned int numEntries;
    unsigned int i;
    int len, len2, sizeToWrite;
    FILE *inFile;
    FILE *baseFile;
    FILE *dxtFile;
    char *buffer;
    int convertDDS = 0;

    printf("Writing to %s\n", argv[1]);
    printf("Overwriting file %s\n", argv[2]);
    if ((baseFile = fopen(argv[1], "rb+")) == NULL) {
        printf("Could not open file for read.\n");
        return -1;
    }
    if ((inFile = fopen(argv[2], "rb")) == NULL) {
        printf("Could not open file for write.\n");
        return -1;
    }
    len2 = strlen(argv[2]);
    if (argv[2][len2 - 3] == 'd' && argv[2][len2 - 2] == 'd' && argv[2][len2 - 1] == 's') {
	argv[2][len2 - 2] = 'x';
	argv[2][len2 - 1] = 't';
	convertDDS = 1;
	if ((dxtFile = fopen(argv[2], "rb")) == NULL) {
	    printf("Could not open file for write.\n");
	    return -1;
	}
    }

    /* Read header */
    fread(&header, sizeof(PakHeader), 1, baseFile);
    /* Read file entries */
    numEntries = header.sizeOfFileEntries / sizeof(PakFileEntry);
    entries = malloc(header.sizeOfFileEntries);

    fseek(baseFile, header.startOfFileEntries, SEEK_SET);
    fread(entries, header.sizeOfFileEntries, 1, baseFile);
    /* Dump files */
    for (i = 0; i < numEntries; i++) {
        fseek(baseFile, entries[i].fileNamePos + header.startOfFileNames, SEEK_SET);
        fgets(fileName, FILENAME_MAX, baseFile);
	if (strcmp(fileName, argv[2]) == 0) {
	    printf("Found %s. Adding ...\n", fileName);
	    fseek(baseFile, entries[i].fileDataPos + header.startOfData, SEEK_SET);
	    fseek(inFile, 0, SEEK_END);
	    len = ftell(inFile);
	    fseek(inFile, 0, SEEK_SET);
	    printf("Input size is %d ...\n", len);
	    if (entries[i].dataSize > len) {
		sizeToWrite = len;
		printf("Writing new file smaller than old file!");
	    } else {
		sizeToWrite = entries[i].dataSize;
	    }
	    printf("Size written will be %d ...\n", sizeToWrite);
	    buffer = malloc(len);
	    /* Store dds files in as dxt */
	    if (convertDDS) {
		printf("Converting dds to dxt by stripping %ld header bytes (to get %ld bytes) and then adding the first 12 bytes from the dxt. Writing %s...\n", sizeof(DDS_header), len - sizeof(DDS_header), fileName);
		fread(buffer, 1, 12, dxtFile);
		len2 = fwrite(buffer, 1, 12, baseFile);
		len = fread(buffer, 1, len, inFile);
		len2 += fwrite(buffer + sizeof(DDS_header), 1, sizeToWrite - 12, baseFile);
	    } else {
		len = fread(buffer, 1, len, inFile);
		len2 = fwrite(buffer, 1, sizeToWrite,  baseFile);
	    }
	    printf("Read %d bytes, wrote %d bytes.\n", len, len2);
	    fclose(inFile);
	    break;
	    free(buffer);
	}
    }
    fseek(baseFile, header.startOfFileEntries, SEEK_SET);
    entries[i].dataSize = len2;
    entries[i].dataSize2 = len2;
    fwrite(entries, header.sizeOfFileEntries, 1, baseFile);
    fclose(baseFile);
    free(entries);
    return 0;
}
