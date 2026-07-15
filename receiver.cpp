#include <iostream>
#include <vector>
#include <map>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <fcntl.h>
#include <sys/time.h>
#include <cstdlib>

struct Packet {
    uint32_t seq;
    uint8_t payload[160];
} __attribute__((packed));

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

double get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

int main() {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int opt = 1;
    setsockopt(in_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (sockaddr *)&in_addr, sizeof(in_addr));
    set_nonblocking(in_fd);

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    int nack_fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47003);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    double t0 = std::stod(getenv("T0") ? getenv("T0") : "0");
    std::map<uint32_t, std::vector<uint8_t>> received;
    std::map<uint32_t, std::vector<uint8_t>> fec_received;
    std::map<uint32_t, double> last_nack_time;
    
    uint32_t max_seq = 0;
    uint8_t buf[2048];

    auto send_to_player = [&](uint32_t seq) {
        Packet pkt;
        pkt.seq = htonl(seq);
        memcpy(pkt.payload, received[seq].data(), 160);
        sendto(out_fd, &pkt, 164, 0, (sockaddr *)&player, sizeof(player));
    };

    auto try_recover = [&](uint32_t base) {
        if (fec_received.count(base)) {
            if (received.count(base) && !received.count(base + 1)) {
                std::vector<uint8_t> recovered(160);
                for (int i = 0; i < 160; ++i) {
                    recovered[i] = received[base][i] ^ fec_received[base][i];
                }
                received[base + 1] = recovered;
                send_to_player(base + 1);
                max_seq = std::max(max_seq, base + 1);
            } else if (!received.count(base) && received.count(base + 1)) {
                std::vector<uint8_t> recovered(160);
                for (int i = 0; i < 160; ++i) {
                    recovered[i] = received[base + 1][i] ^ fec_received[base][i];
                }
                received[base] = recovered;
                send_to_player(base);
                max_seq = std::max(max_seq, base);
            }
        }
    };

    while (true) {
        ssize_t n = recvfrom(in_fd, buf, sizeof(buf), 0, NULL, NULL);
        if (n == 164) {
            uint32_t raw_seq;
            memcpy(&raw_seq, buf, 4);
            raw_seq = ntohl(raw_seq);
            
            if (raw_seq & 0x80000000) {
                uint32_t base = raw_seq & 0x7FFFFFFF;
                std::vector<uint8_t> payload(buf + 4, buf + 164);
                fec_received[base] = payload;
                try_recover(base);
            } else {
                uint32_t seq = raw_seq;
                if (!received.count(seq)) {
                    std::vector<uint8_t> payload(buf + 4, buf + 164);
                    received[seq] = payload;
                    send_to_player(seq);
                    max_seq = std::max(max_seq, seq);
                    try_recover(seq & ~1); // Attempt partner reconstruction
                }
            }
        }

        double now = get_time();
        if (max_seq > 0 && t0 > 0) {
            uint32_t start = max_seq > 80 ? max_seq - 80 : 0;
            for (uint32_t i = start; i < max_seq; ++i) {
                if (!received.count(i)) {
                    double expected_arrival = t0 + (i * 0.020);
                    // If 40ms has elapsed since expected arrival and we still don't have it
                    if (now > expected_arrival + 0.04) {
                        if (now - last_nack_time[i] > 0.035) { // Protect feedback path with 35ms rate limit
                            uint32_t net_i = htonl(i);
                            sendto(nack_fd, &net_i, 4, 0, (sockaddr *)&relay, sizeof(relay));
                            last_nack_time[i] = now;
                        }
                    }
                }
            }
        }
        usleep(1000); // 1ms throttle
    }
    return 0;
}