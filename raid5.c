#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


#define MAX_DISKS 32
#define MAX_BLOCK_SIZE 4096

// Convert a single hex character to its integer value
int hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

// Convert a hex string to bytes
void hex_string_to_bytes(const char *hex, unsigned char *bytes, int byte_count) {
    for (int i = 0; i < byte_count; i++) {
        int high = hex_char_to_value(hex[2 * i]);
        int low = hex_char_to_value(hex[2 * i + 1]); 
        bytes[i] = (high << 4) | low;
    }
}

// Convert bytes to a hex string
void bytes_to_hex_string(const unsigned char *bytes, char *hex, int byte_count) {
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < byte_count; i++) {
        hex[2 * i] = hex_chars[(bytes[i] >> 4) & 0xF];
        hex[2 * i + 1] = hex_chars[bytes[i] & 0xF];
    }
    hex[2 * byte_count] = '\0';
}

// Calculate parity block using XOR
void calculate_parity(unsigned char *parity_block, unsigned char **blocks, int block_size, int num_data_blocks) {
    memset(parity_block, 0, block_size);
    for (int i = 0; i < num_data_blocks; i++) {
        for (int j = 0; j < block_size; j++) {
            parity_block[j] ^= blocks[i][j];
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s B J input_file K disk0 disk1 ... diskN\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Parse arguments
    int B = atoi(argv[1]);
    int J = atoi(argv[2]);
    const char *input_file_path = argv[3];
    int K = atoi(argv[4]);
    int num_disks = argc - 5;

    if (B < 1 || B > MAX_BLOCK_SIZE) {
        fprintf(stderr, "Invalid block size B: %d\n", B);
        return EXIT_FAILURE;
    }
    if (J < 1 || J > B * 10000 || J % B != 0) {
        fprintf(stderr, "Invalid J: %d\n", J);
        return EXIT_FAILURE;
    }
    if (K < 1 || K > B * 10000 || K % B != 0) {
        fprintf(stderr, "Invalid K: %d\n", K);
        return EXIT_FAILURE;
    }
    if (num_disks < 2 || num_disks > MAX_DISKS) {
        fprintf(stderr, "Invalid number of disks: %d\n", num_disks);
        return EXIT_FAILURE;
    }
    if (K * (num_disks - 1) < J) {
        fprintf(stderr, "K*(N-1) must be >= J\n");
        return EXIT_FAILURE;
    }

    // Read input file
    FILE *input_file = fopen(input_file_path, "r");
    if (!input_file) {
        perror("Failed to open input file");
        return EXIT_FAILURE;
    }
    fseek(input_file, 0, SEEK_END);
    long file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    if (file_size != J * 2) {
        fprintf(stderr, "Input file size (%ld) doesn't match J (%d)\n", file_size, J * 2);
        fclose(input_file);
        return EXIT_FAILURE;
    }
    char *input_hex = malloc(J * 2 + 1);
    if (!input_hex) {
        perror("Memory allocation failed");
        fclose(input_file);
        return EXIT_FAILURE;
    }
    fread(input_hex, 1, J * 2, input_file);
    fclose(input_file);

    unsigned char *input_bytes = malloc(J);
    if (!input_bytes) {
        perror("Memory allocation failed");
        free(input_hex);
        return EXIT_FAILURE;
    }
    hex_string_to_bytes(input_hex, input_bytes, J);
    free(input_hex);

    int num_blocks = J / B;
    int blocks_per_disk = K / B;
    int stripes = blocks_per_disk; // Each disk has K/B blocks, so this is the max number of stripes

    // Allocate and zero disk buffers
    unsigned char **disk_blocks = malloc(num_disks * sizeof(unsigned char *));
    for (int i = 0; i < num_disks; i++) {
        disk_blocks[i] = calloc(K, 1);
        if (!disk_blocks[i]) {
            perror("Memory allocation failed");
            for (int j = 0; j < i; j++) free(disk_blocks[j]);
            free(disk_blocks);
            free(input_bytes);
            return EXIT_FAILURE;
        }
    }

    // Distribute blocks and calculate parity
    int input_block_idx = 0;
    for (int stripe = 0; stripe < stripes; stripe++) {
        int parity_disk = (num_disks - 1 - stripe) % num_disks;
        unsigned char **data_blocks = malloc((num_disks - 1) * sizeof(unsigned char *));
        int data_idx = 0;
        for (int disk = 0; disk < num_disks; disk++) {
            if (disk == parity_disk) continue;
            unsigned char *dst = disk_blocks[disk] + stripe * B;
            if (input_block_idx < num_blocks) {
                memcpy(dst, input_bytes + input_block_idx * B, B);
            } else {
                memset(dst, 0, B);
            }
            data_blocks[data_idx++] = dst;
            input_block_idx++;
        }
        // Parity
        calculate_parity(disk_blocks[parity_disk] + stripe * B, data_blocks, B, num_disks - 1);
        free(data_blocks);
    }

    // Write output files
    for (int i = 0; i < num_disks; i++) {
        FILE *f = fopen(argv[5 + i], "w");
        if (!f) {
            perror("Failed to create output file");
            for (int j = 0; j < num_disks; j++) free(disk_blocks[j]);
            free(disk_blocks);
            free(input_bytes);
            return EXIT_FAILURE;
        }
        char *hex_output = malloc(K * 2 + 1);
        bytes_to_hex_string(disk_blocks[i], hex_output, K);
        fwrite(hex_output, 1, K * 2, f);
        fclose(f);
        free(hex_output);
    }

    // Cleanup
    for (int i = 0; i < num_disks; i++) free(disk_blocks[i]);
    free(disk_blocks);
    free(input_bytes);
    return EXIT_SUCCESS;
}