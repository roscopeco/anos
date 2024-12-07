/*
 * Just a little FAT chain dumper; Helps with debugging...
 */
#include <stdint.h>
#include <stdio.h>

#define SECT_SIZE 512

typedef struct {
    char header[3];
    char oemname[8];
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t fatCount;
    uint16_t rootEntryCount;
    uint16_t sectorCount;
    uint8_t mediaDescriptor;
    uint16_t sectorsPerFat;
    uint16_t sectorsPerTrack;
    uint16_t heads;
    uint32_t hiddenSectors;
    uint32_t sectorsInFilesystem;
    uint8_t driveNumber;
    uint8_t reserved;
    uint8_t extendedSignature;
    uint32_t serialNumber;
    char volumeName[11];
    char fsType[8];
} __attribute__((packed)) BPB;

typedef struct {
    char filename[8];
    char fileext[3];
    uint8_t attrs;
    uint8_t unused;
    uint8_t ctimeMs;
    uint16_t ctimeFmt;
    uint16_t cdateFmt;
    uint16_t adateFmt;
    uint16_t eadate;
    uint16_t mtime;
    uint16_t mdate;
    uint16_t cluster;
    uint32_t size;
} __attribute__((packed)) DIRENT;

size_t read_sectors(uint8_t *buffer, uint32_t start, size_t count, FILE *f) {
    uint32_t ofs = start * SECT_SIZE;
    fseek(f, ofs, SEEK_SET);
    return fread(buffer, SECT_SIZE, count, f);
}

size_t clusterToSector(size_t cluster, uint16_t sectorsPerCluster) {
    return (cluster - 2) * sectorsPerCluster;
}

uint16_t fatEntry(size_t cluster, uint8_t *fat) {
    uint16_t raw = *(uint16_t *)(fat + ((cluster >> 1) + cluster));
    return cluster & 0x1 ? raw >> 4 : raw & 0x0FFF;
}

static uint8_t buffer[1048576];

int main(int argc, char **argv) {
    FILE *f = fopen(argc == 1 ? "../floppy.img" : argv[1], "rb");

    if (!f) {
        printf("File not opened\n");
        return -1;
    }

    // Read BPB / boot sect
    read_sectors(buffer, 0, 1, f);

    BPB *bpb = (BPB *)buffer;

    printf("VOLUME: %.11s [%.8s] [%d sectors / cluster]\n", bpb->volumeName,
           bpb->fsType, bpb->sectorsPerCluster);

    uint16_t bytesPerSector = bpb->bytesPerSector;
    uint16_t reservedSectors = bpb->reservedSectors;
    uint16_t sectorsPerFat = bpb->sectorsPerFat;
    uint16_t rootStart = reservedSectors + bpb->fatCount * sectorsPerFat;
    uint16_t sectorsPerCluster = bpb->sectorsPerCluster;

    uint32_t dataStart =
            rootStart + (bpb->rootEntryCount / (bytesPerSector / 0x20));

    printf("Root dir begins at sector %d\n", rootStart);
    printf("Data area begins at sector %d\n", dataStart);

    // Read root dir
    read_sectors(buffer, rootStart, 1, f);

    DIRENT *dirent = (DIRENT *)buffer;

    printf("FILE: %.8s.%.3s\n", dirent->filename, dirent->fileext);

    uint16_t firstCluster = dirent->cluster;
    uint16_t size = dirent->size;

    printf("Begins at cluster %d; Size is %d byte(s)\n", firstCluster, size);

    return 0;
    if (size == 0) {
        return 0;
    }

    // read FAT
    read_sectors(buffer, reservedSectors, sectorsPerFat, f);

    for (int i = 0; i < 0x20; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("\n");

    uint16_t thisCluster = firstCluster;
    uint16_t nextCluster = fatEntry(firstCluster, buffer);
    int tots = bytesPerSector;

    do {
#ifdef DATA_DUMP
        printf("\n\n\n");
        printf("###############################################################"
               "#\n");
#endif

        printf("%-4d ==> %-4d\n", thisCluster, nextCluster);

#ifdef DATA_DUMP
        uint8_t data[sectorsPerCluster * bytesPerSector];
        read_sectors(data,
                     dataStart +
                             clusterToSector(thisCluster, sectorsPerCluster),
                     1, f);

        for (int i = 0; i < bytesPerSector; i++) {
            printf("%c", data[i]);
        }
#endif

        tots += sectorsPerCluster * bytesPerSector;
        thisCluster = nextCluster;
        nextCluster = fatEntry(nextCluster, buffer);
    } while (nextCluster != 0xfff);

    printf("End of chain; %d byte(s) in total sectors\n", tots);
    printf("%d byte(s) wasted based on reported size of %d byte(s)\n",
           tots - size, size);

    fclose(f);
    return 0;
}
