#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <openssl/evp.h>
#include <uthash.h>

#define MAX_CHUNK_SIZE 1024
#define PORT 8888
#define SHA256_HEX_LENGTH 64

// apt install libcurl4-openssl-dev libssl-dev zlib1g-dev uthash-dev
// gcc -o b.out receive_png.c -lssl -lcrypto

// Structure to store chunk data
typedef struct {
    char hash[65];           // 64 hex chars + null terminator
    unsigned char *data;     // Chunk data
    size_t size;            // Size of chunk
    UT_hash_handle hh;      // Makes this structure hashable
} chunk_entry;

chunk_entry *chunks = NULL;  // Global hash table

// Function to calculate SHA256 hash
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
    hash_str[64] = '\0';

    EVP_MD_CTX_free(mdctx);
}

// Function to add chunk to hash table
void add_chunk(unsigned char *data, size_t size) {
    chunk_entry *entry = malloc(sizeof(chunk_entry));
    
    calculate_hash(data, size, entry->hash);
    
    chunk_entry *existing;
    HASH_FIND_STR(chunks, entry->hash, existing);
    if (existing) {
        printf("Duplicate chunk received (hash: %s)\n", entry->hash);
        free(entry);
        return;
    }

    entry->data = malloc(size);
    memcpy(entry->data, data, size);
    entry->size = size;

    HASH_ADD_STR(chunks, hash, entry);
    printf("Added chunk with hash: %s (size: %zu)\n", entry->hash, size);
}

// Function to process hash list and write chunks to file
void process_hash_list(unsigned char *list_data, size_t size) {
    FILE *outfile = fopen("b.png", "ab");
    if (!outfile) {
        perror("Failed to open output file");
        return;
    }

    char hash[65];
    size_t pos = 0;
    int hash_count = 0;

    while (pos < size) {
        // Copy next 64 characters (hash)
        memcpy(hash, list_data + pos, 64);
        hash[64] = '\0';

        // Find chunk with this hash
        chunk_entry *entry;
        HASH_FIND_STR(chunks, hash, entry);
        
        if (entry) {
            // Write chunk to file
            fwrite(entry->data, 1, entry->size, outfile);
            printf("Wrote chunk with hash %s to file\n", hash);
            
            // Remove from hash table and free memory
            HASH_DEL(chunks, entry);
            free(entry->data);
            free(entry);
            hash_count++;
        } else {
            printf("Warning: Missing chunk with hash %s\n", hash);
        }

        // Move to next hash (64 chars + newline)
        pos += 65;
    }

    fclose(outfile);
    printf("Processed %d chunks from hash list\n", hash_count);
}

int main() {
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    unsigned char buffer[MAX_CHUNK_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("UDP server listening on port %d...\n", PORT);

    while (1) {
        ssize_t n = recvfrom(sockfd, buffer, MAX_CHUNK_SIZE, 0,
                            (struct sockaddr *)&client_addr, &client_len);
        
        if (n < 0) {
            perror("Receive error");
            continue;
        }

        // Check if this is a hash list (multiple of 65 bytes)
        if (n % 65 == 0) {
            printf("Received hash list (%zd bytes)\n", n);
            process_hash_list(buffer, n);
        } else {
            // Regular chunk
            add_chunk(buffer, n);
        }
    }

    close(sockfd);
    return 0;
}
