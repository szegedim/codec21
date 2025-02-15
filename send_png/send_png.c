#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/evp.h>

#define MAX_CHUNK_SIZE 1024
#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"
#define SHA256_HEX_LENGTH 64
#define HASHES_PER_GROUP 10

// apt install libcurl4-openssl-dev libssl-dev zlib1g-dev uthash-dev
// gcc -o a.out send_png.c -lssl -lcrypto

// Structure to store hash and data
typedef struct {
    char hash[SHA256_HEX_LENGTH + 2];  // 64 hex chars + newline + null
    size_t size;
} HashEntry;

void calculate_hash(unsigned char *data, size_t size, char *hash_str) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;

    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(mdctx, data, size);
    EVP_DigestFinal_ex(mdctx, hash, &hash_len);

    for(int i = 0; i < hash_len; i++) {
        sprintf(hash_str + (i * 2), "%02x", hash[i]);
    }
    hash_str[SHA256_HEX_LENGTH] = '\n';
    hash_str[SHA256_HEX_LENGTH + 1] = '\0';

    EVP_MD_CTX_free(mdctx);
}

int main() {
    FILE *file = fopen("a.png", "rb");
    if (!file) {
        perror("Failed to open file");
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        fclose(file);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Allocate buffers
    unsigned char *chunk_buffer = malloc(MAX_CHUNK_SIZE);
    HashEntry *hash_entries = NULL;
    size_t hash_count = 0;
    size_t hash_capacity = 16;  // Initial capacity
    size_t bytes_read;
    int chunk_number = 0;

    // Allocate initial hash entries array
    hash_entries = malloc(hash_capacity * sizeof(HashEntry));
    if (!hash_entries || !chunk_buffer) {
        perror("Memory allocation failed");
        goto cleanup;
    }

    // First pass: send chunks and collect hashes
    while ((bytes_read = fread(chunk_buffer, 1, MAX_CHUNK_SIZE, file)) > 0) {
        // Calculate hash
        if (hash_count >= hash_capacity) {
            hash_capacity *= 2;
            HashEntry *temp = realloc(hash_entries, hash_capacity * sizeof(HashEntry));
            if (!temp) {
                perror("Memory reallocation failed");
                goto cleanup;
            }
            hash_entries = temp;
        }

        calculate_hash(chunk_buffer, bytes_read, hash_entries[hash_count].hash);
        hash_entries[hash_count].size = bytes_read;
        hash_count++;

        // Send the chunk
        if (sendto(sock, chunk_buffer, bytes_read, 0,
                   (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Failed to send chunk");
            goto cleanup;
        }
        printf("Sent chunk %d with %zu bytes\n", ++chunk_number, bytes_read);
        usleep(1000);
    }

    // Send hash lists in groups
    char group_buffer[(SHA256_HEX_LENGTH + 1) * HASHES_PER_GROUP + 1];
    size_t current_hash = 0;

    while (current_hash < hash_count) {
        size_t hashes_this_group = (hash_count - current_hash < HASHES_PER_GROUP) ? 
                                  (hash_count - current_hash) : HASHES_PER_GROUP;
        
        // Build group buffer
        group_buffer[0] = '\0';
        for (size_t i = 0; i < hashes_this_group; i++) {
            strcat(group_buffer, hash_entries[current_hash + i].hash);
        }

        // Send hash group
        size_t group_size = strlen(group_buffer);
        if (sendto(sock, group_buffer, group_size, 0,
                   (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Failed to send hash group");
            goto cleanup;
        }
        printf("Sent hash group with %zu hashes\n", hashes_this_group);
        
        current_hash += hashes_this_group;
        usleep(1000);
    }

cleanup:
    free(chunk_buffer);
    free(hash_entries);
    fclose(file);
    close(sock);
    return 0;
}
