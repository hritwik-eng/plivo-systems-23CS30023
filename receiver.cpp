#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#define MAX_FRAMES 65536
#define PAYLOAD_BYTES 160

static uint8_t payloads[MAX_FRAMES][PAYLOAD_BYTES];
static bool received[MAX_FRAMES];
static bool sent_to_player[MAX_FRAMES];

static double get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr);
    fcntl(in_fd, F_SETFL, O_NONBLOCK);

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    double t0 = atof(getenv("T0") ? getenv("T0") : "0");
    unsigned char buf[2048], out_buf[2048];

    for (;;) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(in_fd, &read_fds);
        struct timeval tv = {0, 1000};
        select(in_fd + 1, &read_fds, NULL, NULL, &tv);

        if (FD_ISSET(in_fd, &read_fds)) {
            ssize_t n = recvfrom(in_fd, (char *)buf, sizeof buf, 0, NULL, NULL);
            if (n >= 5) {
                uint8_t type = buf[0];
                uint32_t net_seq;
                memcpy(&net_seq, buf + 1, 4);
                uint32_t seq = ntohl(net_seq);

                // Handle Primary Packet
                if (seq < MAX_FRAMES && !received[seq]) {
                    memcpy(payloads[seq], buf + 5, PAYLOAD_BYTES);
                    received[seq] = true;
                }

                // Handle Redundant Piggyback (Type 5)
                if (type == 5 && seq >= 4 && n >= 5 + 2 * PAYLOAD_BYTES) {
                    uint32_t red_seq = seq - 4;
                    if (red_seq < MAX_FRAMES && !received[red_seq]) {
                        memcpy(payloads[red_seq], buf + 5 + PAYLOAD_BYTES, PAYLOAD_BYTES);
                        received[red_seq] = true;
                    }
                }
            }
        }

        // Send to player at deadline
        double now = get_time();
        int max_s = (int)((now - t0) * 50.0);
        for (int s = 0; s <= max_s && s < MAX_FRAMES; s++) {
            if (received[s] && !sent_to_player[s]) {
                uint32_t net_s = htonl(s);
                memcpy(out_buf, &net_s, 4);
                memcpy(out_buf + 4, payloads[s], PAYLOAD_BYTES);
                sendto(out_fd, (char *)out_buf, 4 + PAYLOAD_BYTES, 0, (struct sockaddr *)&player, sizeof player);
                sent_to_player[s] = true;
            }
        }
    }
    return 0;
}