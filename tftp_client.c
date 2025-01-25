#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#define TFTP_PORT 69
#define BUFFER_SIZE 516 // 512 bytes data + 4 bytes header
#define BLOCK_SIZE 512  // Block size for TFTP

// TFTP opcode definitions
#define OP_WRQ 2
#define OP_DATA 3
#define OP_ACK 4

void send_wrq(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len, const char *filename);
void send_file(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len, const char *filename);

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <file_to_send>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_ip = argv[1];
    const char *filename = argv[2];

    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TFTP_PORT);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Send WRQ
    send_wrq(sockfd, &server_addr, addr_len, filename);

    // Send file data
    send_file(sockfd, &server_addr, addr_len, filename);

    close(sockfd);
    return 0;
}

void send_wrq(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len, const char *filename) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    // Build WRQ packet
    buffer[0] = 0;
    buffer[1] = OP_WRQ;  // Opcode for WRQ
    strcpy(&buffer[2], filename);         // Filename
    strcpy(&buffer[2 + strlen(filename) + 1], "octet"); // Mode (octet = binary)

    int len = 2 + strlen(filename) + 1 + strlen("octet") + 1;

    // Send WRQ packet
    if (sendto(sockfd, buffer, len, 0, (struct sockaddr *)server_addr, addr_len) < 0) {
        perror("Error sending WRQ");
        exit(EXIT_FAILURE);
    }

    printf("WRQ sent for file: %s\n", filename);
}

void send_file(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    int block_num = 1;
    int bytes_read;

    while ((bytes_read = fread(&buffer[4], 1, BLOCK_SIZE, file)) > 0) {
        // Build DATA packet
        buffer[0] = 0;
        buffer[1] = OP_DATA; // Opcode for DATA
        buffer[2] = (block_num >> 8) & 0xFF; // Block number (high byte)
        buffer[3] = block_num & 0xFF;        // Block number (low byte)

        // Send DATA packet
        if (sendto(sockfd, buffer, bytes_read + 4, 0, (struct sockaddr *)server_addr, addr_len) < 0) {
            perror("Error sending DATA packet");
            fclose(file);
            exit(EXIT_FAILURE);
        }

        printf("Sent block %d (%d bytes)\n", block_num, bytes_read);

        // Wait for ACK
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)server_addr, &addr_len);
        if (n < 0) {
            perror("Error receiving ACK");
            fclose(file);
            exit(EXIT_FAILURE);
        }

        if (buffer[1] == OP_ACK) {
            int ack_block_num = (buffer[2] << 8) | buffer[3];
            if (ack_block_num == block_num) {
                printf("Received ACK for block %d\n", block_num);
                block_num++;
            } else {
                printf("Unexpected ACK block number: %d\n", ack_block_num);
            }
        }
    }

    fclose(file);
    printf("File transfer completed.\n");
}
