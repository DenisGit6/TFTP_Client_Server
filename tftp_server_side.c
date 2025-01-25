#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>


// Macros for project
#define TFTP_PORT 69      //This is for the UDP port
#define BUFFER_SIZE 516  // 512 bytes data + 4 bytes header
#define BLOCK_SIZE 512   // Block size for TFTP

void show_progress(const char *filename, long total_size, long received_size);
void handle_file_transfer(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len);

int main() {
    int sockfd; //file descriptor for the UDP
    struct sockaddr_in server_addr, client_addr; //holds the adress of client-server
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); //APv4 address only
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(TFTP_PORT);

    // Bind the socket to the TFTP port
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("TFTP Server with Progress GUI is running on port %d\n", TFTP_PORT);

    // Listen for client requests
    handle_file_transfer(sockfd, &client_addr, client_len);

    close(sockfd);
    return 0;
}

void handle_file_transfer(int sockfd, struct sockaddr_in *client_addr, socklen_t client_len) {
    char buffer[BUFFER_SIZE];
    char filename[256] = "received_file"; // Default filename
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error opening file");
        return;
    }

    long total_size = 0;   // Total file size (can be provided by the client or estimated)
    long received_size = 0; // Bytes received so far
    int block_num = 1;

    printf("Ready to receive file...\n");

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)client_addr, &client_len);
        if (n < 0) {
            perror("Error receiving data");
            break;
        }

        // If DATA packet
        if (buffer[1] == 3) { // OP_DATA
            int received_block = (buffer[2] << 8) | buffer[3];
            if (received_block == block_num) {
                // Write data to file
                fwrite(&buffer[4], 1, n - 4, file);
                received_size += n - 4;

                // Send ACK
                buffer[0] = 0;
                buffer[1] = 4; // OP_ACK
                buffer[2] = (block_num >> 8) & 0xFF;
                buffer[3] = block_num & 0xFF;

                sendto(sockfd, buffer, 4, 0, (struct sockaddr *)client_addr, client_len);
                block_num++;

                // Update progress in GUI
                show_progress(filename, total_size, received_size);

                printf("Block %d received (%ld bytes)\n", received_block, received_size);

                // If last block (less than 512 bytes)
                if (n < BUFFER_SIZE) {
                    printf("File transfer completed.\n");
                    break;
                }
            }
        }
    }

    fclose(file);
}

void show_progress(const char *filename, long total_size, long received_size) {
    GtkWidget *window;
    GtkWidget *progress_bar;
    GtkWidget *vbox;
    char title[256];
    double progress = (total_size > 0) ? (double)received_size / total_size : 0.0;

    gtk_init(NULL, NULL);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "File Transfer Progress");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 100);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    snprintf(title, sizeof(title), "Receiving File: %s", filename);
    GtkWidget *label = gtk_label_new(title);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 5);

    progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), progress);
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, TRUE, TRUE, 5);

    char progress_text[50];
    snprintf(progress_text, sizeof(progress_text), "%.2f%% (%ld / %ld bytes)",
             progress * 100, received_size, total_size);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), progress_text);
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar), TRUE);

    gtk_widget_show_all(window);

    // Simulate a non-blocking GUI
    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
}
