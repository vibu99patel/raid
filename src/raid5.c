#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BLOCK_SIZE 4096

int hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

void hex_string_to_bytes(const char *hex, unsigned char *bytes, int byte_count) {
    for (int i = 0; i < byte_count; i++) {
        int high = hex_char_to_value(hex[2 * i]);
        int low = hex_char_to_value(hex[2 * i + 1]);
        bytes[i] = (high << 4) | low;
    }
}

void bytes_to_hex_string(const unsigned char *bytes, char *hex, int byte_count) {
    static const char hex_chars[] = "0123456789abcdef";
    for (int i = 0; i < byte_count; i++) {
        hex[2 * i] = hex_chars[(bytes[i] >> 4) & 0xF];
        hex[2 * i + 1] = hex_chars[bytes[i] & 0xF];
    }
    hex[2 * byte_count] = '\0';
}

void calculate_parity(unsigned char *parity_block, unsigned char **blocks, int block_size, int num_blocks) {
    memset(parity_block, 0, block_size);
    for (int i = 0; i < num_blocks; i++) {
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

    int B = atoi(argv[1]);
    int J = atoi(argv[2]);
    const char *input_file_path = argv[3];
    int K = atoi(argv[4]);
    int N = argc - 5;

    if (B < 1 || B > MAX_BLOCK_SIZE || J < 1 || K < 1 || N < 2) {
        fprintf(stderr, "Invalid parameters\n");
        return EXIT_FAILURE;
    }

    // Read input file
    FILE *input_file = fopen(input_file_path, "r");
    if (!input_file) {
        perror("Failed to open input file");
        return EXIT_FAILURE;
    }
    char *input_hex = malloc(J * 2 + 1);
    fread(input_hex, 1, J * 2, input_file);
    fclose(input_file);

    unsigned char *input_bytes = malloc(J);
    hex_string_to_bytes(input_hex, input_bytes, J);
    free(input_hex);

    // Prepare disk buffers
    unsigned char **disks = malloc(N * sizeof(unsigned char *));
    for (int i = 0; i < N; i++) {
        disks[i] = calloc(K, 1);
    }

    int num_blocks = J / B;
    int block_idx = 0;
    int stripe = 0;

    while (block_idx < num_blocks) {
        int parity_disk = (N - 1 - stripe % N) % N;  // Rotating parity
        
        // Prepare data blocks for this stripe
        unsigned char **data_blocks = malloc((N - 1) * sizeof(unsigned char *));
        for (int i = 0; i < N - 1; i++) {
            data_blocks[i] = malloc(B);
            if (block_idx < num_blocks) {
                memcpy(data_blocks[i], input_bytes + block_idx * B, B);
                block_idx++;
            } else {
                memset(data_blocks[i], 0, B);
            }
        }

        // Calculate parity
        calculate_parity(disks[parity_disk] + stripe * B, data_blocks, B, N - 1);

        // Distribute data blocks to non-parity disks
        int disk_idx = (parity_disk + 1) % N;
        for (int i = 0; i < N - 1; i++) {
            memcpy(disks[disk_idx] + stripe * B, data_blocks[i], B);
            disk_idx = (disk_idx + 1) % N;
            free(data_blocks[i]);
        }
        free(data_blocks);
        stripe++;
    }

    // Write disks to output files
    for (int i = 0; i < N; i++) {
        FILE *f = fopen(argv[5 + i], "w");
        if (!f) {
            perror("Failed to open output file");
            return EXIT_FAILURE;
        }
        char *hex_output = malloc(K * 2 + 1);
        bytes_to_hex_string(disks[i], hex_output, K);
        fwrite(hex_output, 1, K * 2, f);
        fclose(f);
        free(hex_output);
    }

    // Free memory
    for (int i = 0; i < N; i++) {
        free(disks[i]);
    }
    free(disks);
    free(input_bytes);

    return EXIT_SUCCESS;
}