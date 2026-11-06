#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr int worker_count = 2;

void write_all(int fd, const std::string& message) {
    size_t pos = 0;
    while (pos < message.size()) {
        ssize_t written = ::send(fd, message.data() + pos, message.size() - pos, MSG_NOSIGNAL);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) throw std::runtime_error("send failed/disconnected");
        pos += static_cast<size_t>(written);
    }
}

std::string read_line(int fd) {
    std::string line;
    char c;
    while (line.size() < 256) {
        ssize_t n = ::recv(fd, &c, 1, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) throw std::runtime_error("worker disconnected while reading");
        if (c == '\n') return line;
        line += c;
    }
    throw std::runtime_error("response too long");
}

uint64_t parse_number(const char* s, uint64_t min, uint64_t max) {
    std::string v(s);
    if (v.empty() || v.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("expected positive integer");
    size_t used = 0;
    auto n = std::stoull(v, &used);
    if (used != v.size() || n < min || n > max)
        throw std::runtime_error("integer outside supported range");
    return n;
}

struct Result { uint64_t count = 0; std::string error; };
}

int main(int argc, char** argv) {
    try {
        if (argc > 3) { std::cerr << "Usage: cluster-controller [limit=1000000] [port=5050]\n"; return 2; }
        const uint64_t limit = argc > 1 ? parse_number(argv[1], 2, 100000000) : 1000000;
        const int port = argc > 2 ? static_cast<int>(parse_number(argv[2], 1024, 65535)) : 5050;
        int server = ::socket(AF_INET, SOCK_STREAM, 0);
        if (server < 0) throw std::runtime_error("socket failed");
        int reuse = 1;
        ::setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(static_cast<uint16_t>(port));
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (::bind(server, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::cerr << "bind failed: " << std::strerror(errno) << '\n'; ::close(server); return 1;
        }
        if (::listen(server, worker_count) < 0) throw std::runtime_error("listen failed");
        std::cout << "Controller listening on TCP port " << port << "; waiting for " << worker_count << " workers...\n" << std::flush;
        std::vector<int> workers;
        for (int i = 0; i < worker_count; ++i) {
            int fd = ::accept(server, nullptr, nullptr);
            if (fd < 0) throw std::runtime_error("accept failed");
            workers.push_back(fd);
            std::cout << "Worker " << i + 1 << " connected\n" << std::flush;
        }
        ::close(server);
        uint64_t middle = limit / 2;
        std::vector<Result> results(worker_count);
        std::vector<std::thread> threads;
        auto started = std::chrono::steady_clock::now();
        for (int i = 0; i < worker_count; ++i) {
            uint64_t low = i == 0 ? 1 : middle + 1;
            uint64_t high = i == 0 ? middle : limit;
            std::cout << "Assigning worker " << i + 1 << " range [" << low << ", " << high << "]\n";
            threads.emplace_back([&, i, low, high] {
                try {
                    write_all(workers[i], "COUNT " + std::to_string(low) + " " + std::to_string(high) + "\n");
                    auto response = read_line(workers[i]);
                    if (response.rfind("RESULT ", 0) != 0) throw std::runtime_error("invalid reply: " + response);
                    auto str = response.substr(7);
                    results[i].count = parse_number(str.c_str(), 0, limit);
                } catch (const std::exception& e) { results[i].error = e.what(); }
                ::close(workers[i]);
            });
        }
        for (auto& t : threads) t.join();
        for (int i = 0; i < worker_count; ++i) {
            if (!results[i].error.empty()) {
                std::cerr << "Worker " << i + 1 << " failed: " << results[i].error << '\n';
                return 1;
            }
        }
        uint64_t total = results[0].count + results[1].count;
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        std::cout << "Worker 1 count: " << results[0].count << '\n'
                  << "Worker 2 count: " << results[1].count << '\n'
                  << "Total primes in [1, " << limit << "]: " << total << '\n'
                  << "Distributed processing time: " << ms << " ms\n";
    } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; }
}
