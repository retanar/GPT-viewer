#include <stdio.h>
#include <stdlib.h>

struct GPTHeader {
    char signature[8];
    unsigned int revision;
    unsigned int size;
    unsigned int crc32;
    unsigned int reserved;
    unsigned long long int lba_current;
    unsigned long long int lba_backup;
    unsigned long long int lba_usab_first;
    unsigned long long int lba_usab_last;
    unsigned char GUID[16];
    unsigned long long int part_array_start;
    unsigned int entry_count;
    unsigned int entry_size;
    unsigned int entry_crc32;
} gpt_header;

struct GUIDPartition {
    unsigned char GUID_type[16];
    unsigned char GUID[16];
    unsigned long long int lba_first;
    unsigned long long int lba_last;
    unsigned long long int flags;
    unsigned short int name[36];
};

void print_guid(unsigned char guid[]) {
    printf("%02X%02X%02X%02X-", guid[3], guid[2], guid[1], guid[0]);
    printf("%02X%02X-", guid[5], guid[4]);
    printf("%02X%02X-", guid[7], guid[6]);
    printf("%02X%02X-", guid[8], guid[9]);
    printf("%02X%02X%02X%02X%02X%02X", guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]);
}

void print_gpt(struct GPTHeader *h) {
    printf("Signature: %s; Revision: %d\n", h->signature, h->revision);
    printf("GUID: ");
    print_guid(h->GUID);
    printf("\nCurrent LBA: %d; Backup LBA: %d\n", h->lba_current, h->lba_backup);
    printf("First usable LBA: %d; Last usable LBA: %d\n", h->lba_usab_first, h->lba_usab_last);
    printf("Starting LBA of partition array: %d; Entries: %d; Entry size: %d\n", h->part_array_start, h->entry_count, h->entry_size);
}

int utf16_to_utf8(unsigned short int utf16data[], int size, char *out) {
    int pointer = 0;

    for (int i = 0;i<size;i++) {
        int code = 0;
        // Decode utf-16 to codepoint
        if (utf16data[i] <= 0xD7FF || utf16data[i] >= 0xE000)
            code = utf16data[i];
        else {
            code = ((int)utf16data[i] - 0xD800) << 10;
            i++;
            code += (utf16data[i] - 0xDC00);
            code += 0x10000;
        }

        // Encode codepoint to utf-8
        if (code <= 0x7f) {
            out[pointer] = (char) code;
            pointer += 1;
        } else if (code <= 0x7FF) {
            out[pointer]   = 0xC0 | (code >> 6);
            out[pointer+1] = 0x80 | (code & 0x3F);
            pointer += 2;
        } else if (code <= 0xFFFF) {
            out[pointer]   = 0xE0 | (code >> 12);
            out[pointer+1] = 0x80 | ((code >> 6) & 0x3F);
            out[pointer+2] = 0x80 | (code & 0x3F);
            pointer += 3;
        } else if (code <= 0x10FFFF) {
            out[pointer]   = 0xF0 | (code >> 18);
            out[pointer+1] = 0x80 | ((code >> 12) & 0x3F);
            out[pointer+2] = 0x80 | ((code >> 6) & 0x3F);
            out[pointer+3] = 0x80 | (code & 0x3F);
            pointer += 4;
        }
    }
    return pointer;
}

void print_partition(struct GUIDPartition *p) {
    char *utf8str = (char*)malloc(2 * sizeof(p->name) + 1);
    int str_size = utf16_to_utf8(p->name, sizeof(p->name)/sizeof(short int), utf8str);
    utf8str = realloc(utf8str, str_size + 1);

    printf("Partition \"%s\"\n", utf8str);
    free(utf8str);

    printf("GUID type: ");
    print_guid(p->GUID_type);
    printf("\nGUID: ");
    print_guid(p->GUID);
    printf("\nFirst LBA: %u; Last LBA: %u\n", p->lba_first, p->lba_last);
    printf("Flags: %016X\n", p->flags);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Program requires one argument: file path.\n");
        return 1;
    }

    FILE *sda = fopen(argv[1], "rb");
    if (!sda) {
        printf("%s is not a readable file.\n", argv[1]);
        return 1;
    }

    // fseek to skip protective mbr
    fseek(sda, 512, SEEK_SET);
    fread(&gpt_header, 512, 1, sda);
    
    struct GUIDPartition parts[128];
    fread(parts, sizeof(struct GUIDPartition), 128, sda);
    fclose(sda);

    print_gpt(&gpt_header);

    printf("\nPartitions:\n\n");
    for (int i = 0;i<128;i++) {
        if (parts[i].lba_first + parts[i].lba_last == 0)
            break;
        print_partition(&parts[i]);
        printf("\n");
    }

    return 0;
}
