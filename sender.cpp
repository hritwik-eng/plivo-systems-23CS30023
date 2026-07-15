#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_FRAMES 65536
#define PAYLOAD_BYTES 160

static uint8_t history[MAX_FRAMES][PAYLOAD_BYTES];
static bool has_history[MAX_FRAMES];

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr);
    fcntl(in_fd, F_SETFL, O_NONBLOCK);

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];
    unsigned char out_buf[2048];

    for (;;) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(in_fd, &read_fds);
        struct timeval tv = {0, 1000};
        
        if (select(in_fd + 1, &read_fds, NULL, NULL, &tv) < 0) continue;

        if (FD_ISSET(in_fd, &read_fds)) {
            ssize_t n = recvfrom(in_fd, (char *)buf, sizeof buf, 0, NULL, NULL);
            if (n < 4) continue;

            uint32_t net_seq;
            memcpy(&net_seq, buf, 4);
            uint32_t seq = ntohl(net_seq);

            if (seq < MAX_FRAMES && n >= 4 + PAYLOAD_BYTES) {
                memcpy(history[seq], buf + 4, PAYLOAD_BYTES);
                has_history[seq] = true;
            }

            // Offset by 4 frames to survive burst loss
            if (seq >= 4 && (seq % 25 != 0) && has_history[seq - 4]) {
                out_buf[0] = 5; 
                memcpy(out_buf + 1, &net_seq, 4);
                memcpy(out_buf + 5, buf + 4, PAYLOAD_BYTES);
                memcpy(out_buf + 5 + PAYLOAD_BYTES, history[seq - 4], PAYLOAD_BYTES);
                sendto(out_fd, (char *)out_buf, 5 + 2 * PAYLOAD_BYTES, 0, (struct sockaddr *)&relay, sizeof relay);
            } else {
                out_buf[0] = 1;
                memcpy(out_buf + 1, &net_seq, 4);
                memcpy(out_buf + 5, buf + 4, PAYLOAD_BYTES);
                sendto(out_fd, (char *)out_buf, 5 + PAYLOAD_BYTES, 0, (struct sockaddr *)&relay, sizeof relay);
            }
        }
    }
    return 0;
}