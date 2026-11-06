#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void write_all(int fd, const std::string& message) {
    size_t pos = 0;
    while (pos < message.size()) {
        ssize_t n = ::send(fd, message.data() + pos, message.size() - pos, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) throw std::runtime_error("send failed");
        pos += static_cast<size_t>(n);
    }
}
std::string read_line(int fd) {
    std::string s;
    char c;
    while (s.size() < 256) {
        ssize_t n = ::recv(fd, &c, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) throw std::runtime_error("controller disconnected");
        if (c == '\n') return s;
        s += c;
    }
    throw std::runtime_error("request too long");
}
bool is_prime(uint64_t n) {
    if (n < 2) return false;
    for (uint64_t d = 2; d <= n / d; ++d)
        if (n % d == 0) return false;
    return true;
}
uint64_t count_primes(uint64_t first, uint64_t last) {
    uint64_t count = 0;
    for (uint64_t n = first; n <= last; ++n) if (is_prime(n)) ++count;
    return count;
}
}
int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: cluster-worker <controller-ip-or-hostname> [port=5050]\n"; return 2;
    }
    try {
        const std::string port = argc == 3 ? argv[2] : "5050";
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* addresses = nullptr;
        int rc = ::getaddrinfo(argv[1], port.c_str(), &hints, &addresses);
        if (rc != 0) throw std::runtime_error(gai_strerror(rc));
        int fd = -1;
        for (addrinfo* p = addresses; p; p = p->ai_next) {
            fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (fd == -1) continue;
            if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) break;
            ::close(fd);
            fd = -1;
        }
        ::freeaddrinfo(addresses);
        if (fd < 0) throw std::runtime_error("unable to connect to controller");
        std::cout << "Connected to controller; awaiting task...\n";
        std::string request = read_line(fd);
        std::istringstream input(request);
        std::string cmd, extra;
        uint64_t first, last;
        if (!(input >> cmd >> first >> last) || cmd != "COUNT" || (input >> extra) || first < 1 || last < first || last > 100000000)
            throw std::runtime_error("invalid task: " + request);
        std::cout << "Counting primes from " << first << " to " << last << "...\n";
        uint64_t result = count_primes(first, last);
        write_all(fd, "RESULT " + std::to_string(result) + "\n");
        std::cout << "Result sent: " << result << '\n';
        ::close(fd);
    } catch (const std::exception& e) { std::cerr << "Worker error: " << e.what() << '\n'; return 1; }
}
