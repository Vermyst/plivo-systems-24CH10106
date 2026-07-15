#include <iostream>
#include <vector>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>

struct Packet {
    uint32_t seq;
    uint8_t payload[160];
} __attribute__((packed));

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    setsockopt(in_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (sockaddr *)&in_addr, sizeof(in_addr));

    int nack_fd = socket(AF_INET, SOCK_DGRAM, 0);
    setsockopt(nack_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in nack_addr = {0};
    nack_addr.sin_family = AF_INET;
    nack_addr.sin_port = htons(47004);
    nack_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(nack_fd, (sockaddr *)&nack_addr, sizeof(nack_addr));
    set_nonblocking(nack_fd);

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    std::map<uint32_t, std::vector<uint8_t>> history;
    uint8_t buf[2048];

    while (true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(in_fd, &read_fds);
        FD_SET(nack_fd, &read_fds);
        
        int max_fd = std::max(in_fd, nack_fd);
        struct timeval tv = {0, 1000}; // 1ms select timeout
        select(max_fd + 1, &read_fds, NULL, NULL, &tv);

        if (FD_ISSET(in_fd, &read_fds)) {
            ssize_t n = recvfrom(in_fd, buf, sizeof(buf), 0, NULL, NULL);
            if (n == 164) {
                uint32_t seq;
                memcpy(&seq, buf, 4);
                seq = ntohl(seq);
                
                std::vector<uint8_t> payload(buf + 4, buf + 164);
                history[seq] = payload;

                // Transmit the primary media packet
                sendto(out_fd, buf, 164, 0, (sockaddr *)&relay, sizeof(relay));

                // Formulate and send FEC packet for every even-odd pair
                if (seq % 2 == 1 && history.count(seq - 1)) {
                    Packet fec_pkt;
                    fec_pkt.seq = htonl((seq - 1) | 0x80000000); // MSB set flags FEC
                    for (int i = 0; i < 160; ++i) {
                        fec_pkt.payload[i] = history[seq - 1][i] ^ history[seq][i];
                    }
                    sendto(out_fd, &fec_pkt, 164, 0, (sockaddr *)&relay, sizeof(relay));
                }
                
                // Clear state older than 200 packets to limit memory growth
                if (seq > 200) history.erase(seq - 200);
            }
        }

        if (FD_ISSET(nack_fd, &read_fds)) {
            ssize_t n = recvfrom(nack_fd, buf, sizeof(buf), 0, NULL, NULL);
            if (n == 4) {
                uint32_t missing_seq;
                memcpy(&missing_seq, buf, 4);
                missing_seq = ntohl(missing_seq);
                
                if (history.count(missing_seq)) {
                    Packet resend_pkt;
                    resend_pkt.seq = htonl(missing_seq);
                    memcpy(resend_pkt.payload, history[missing_seq].data(), 160);
                    sendto(out_fd, &resend_pkt, 164, 0, (sockaddr *)&relay, sizeof(relay));
                }
            }
        }
    }
    return 0;
}