// Exercise the real client + UpdateChannel + compiled helper. The loader shim
// consumes the ELF upload, then runs the helper on a private socketpair.
#include "../gl/torrent_usb_cache.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;
struct Loader { int peer = -1; pid_t child = -1; size_t upload = 0; };
static std::map<int, Loader> loaders;
static const char* helper;
static int opens = 0, closes = 0;
extern "C" {
int sceNetSocket(const char*, int family, int type, int protocol) {
    assert(family == 2 && type == 1 && protocol == 6);
    int fds[2]; if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds)) return -1;
    loaders[fds[0]] = {fds[1], -1, fs::file_size(helper)}; ++opens; return fds[0];
}
int sceNetSetsockopt(int fd, int, int option, const void* value, unsigned size) {
    assert(size == sizeof(int));
    if (option == 0x1105 || option == 0x1106) {
        const int micros = *static_cast<const int*>(value);
        timeval timeout{micros / 1000000, micros % 1000000};
        return setsockopt(fd, SOL_SOCKET, option == 0x1105 ? SO_SNDTIMEO : SO_RCVTIMEO, &timeout, sizeof(timeout));
    }
    return 0;
}
int sceNetConnect(int fd, const void* address, unsigned size) {
    assert(size == 16); const auto* bytes = static_cast<const unsigned char*>(address);
    assert(bytes[1] == 2 && bytes[2] == (9021 >> 8) && bytes[3] == (9021 & 255));
    assert(bytes[4] == 127 && bytes[5] == 0 && bytes[6] == 0 && bytes[7] == 1);
    auto& loader = loaders.at(fd);
    loader.child = fork();
    if (loader.child == 0) {
        dup2(loader.peer, STDIN_FILENO); dup2(loader.peer, STDOUT_FILENO);
        for (const auto& [socket, item] : loaders) { close(socket); if (item.peer >= 0) close(item.peer); }
        execl(helper, helper, static_cast<char*>(nullptr)); _exit(127);
    }
    close(loader.peer); loader.peer = -1; return loader.child < 0 ? -1 : 0;
}
int sceNetSend(int fd, const void* data, size_t size, int) {
    auto& loader = loaders.at(fd);
    if (loader.upload) { const auto n = std::min(loader.upload, size); loader.upload -= n; return int(n); }
    return int(send(fd, data, size, MSG_NOSIGNAL));
}
int sceNetRecv(int fd, void* data, size_t size, int) { return int(recv(fd, data, size, 0)); }
int sceNetSocketClose(int fd) {
    const auto it = loaders.find(fd); if (it == loaders.end()) return -1;
    close(fd); if (it->second.peer >= 0) close(it->second.peer);
    const pid_t pid = it->second.child;
    if (pid > 0) {
        int status = 0;
        for (int i = 0; i < 100; ++i) {
            if (waitpid(pid, &status, WNOHANG) == pid) break;
            usleep(10000);
            if (i == 99) { kill(pid, SIGKILL); waitpid(pid, &status, 0); }
        }
    }
    loaders.erase(it); ++closes; return 0;
}
}
int main(int argc, char** argv) {
    assert(argc == 2); helper = argv[1];
    std::signal(SIGPIPE, SIG_IGN);
    char path[] = "/tmp/nuvio-usb-cache-XXXXXX"; assert(mkdtemp(path));
    const fs::path root(path); fs::create_directory(root / "usb0"); fs::create_directory(root / "usb7");
    setenv("NUVIO_USB_CACHE_TEST_ROOT", path, 1); setenv("NUVIO_USB_CACHE_TEST_HELPER", helper, 1);
    const auto factory = sx::torrent_usb_cache_factory(); std::string error;
    auto first = factory(0, int64_t(6) << 30, error); assert(first && error.empty());
    assert(fs::is_empty(root / "usb0/.nuvio-maxd-cache")); // Anonymous before first write.
    const std::string payload(512 * 1024 + 3, 'a'); std::string actual(payload.size(), '\0');
    assert(first->write(0, payload.data(), payload.size()));
    assert(first->read(0, actual.data(), actual.size()) && actual == payload);
    assert(first->storage_bytes() > 0 && first->storage_bytes() < (2 << 20));
    const char last = 'z'; char byte = 0;
    assert(first->write((int64_t(6) << 30) - 1, &last, 1));
    assert(first->read((int64_t(6) << 30) - 1, &byte, 1) && byte == last);
    assert(!first->write(int64_t(6) << 30, &last, 1));
    assert(!first->read(-1, &byte, 1));
    auto second = factory(0, int64_t(1) << 30, error); assert(second);
    const char other = 'b'; assert(second->write(0, &other, 1));
    assert(first->read(0, &byte, 1) && byte == 'a'); // Separate sessions/profiles.
    second.reset(); assert(fs::is_empty(root / "usb0/.nuvio-maxd-cache"));
    for (auto& [fd, loader] : loaders) { (void)fd; kill(loader.child, SIGKILL); }
    const auto began = std::chrono::steady_clock::now();
    assert(!first->read(0, &byte, 1)); first.reset();
    assert(std::chrono::steady_clock::now() - began < std::chrono::seconds(3));
    auto seventh = factory(7, int64_t(1) << 30, error); assert(seventh); seventh.reset();
    assert(!factory(1, int64_t(1) << 30, error)); assert(!fs::exists(root / "usb1"));
    fs::create_directory(root / "outside"); std::ofstream(root / "outside/keep") << "unchanged";
    fs::create_directory_symlink(root / "outside", root / "usb2");
    assert(!factory(2, int64_t(1) << 30, error));
    fs::create_directory(root / "usb3"); fs::create_directory_symlink(root / "outside", root / "usb3/.nuvio-maxd-cache");
    assert(!factory(3, int64_t(1) << 30, error));
    assert(fs::file_size(root / "outside/keep") == 9 && !fs::exists(root / "outside/.nuvio-maxd-cache"));
    assert(opens == closes && loaders.empty()); fs::remove_all(root);
    std::cout << "USB cache: real helper/client, fragmented binary I/O, sparse6GiB bounds, session isolation, anonymous cleanup, disconnection, unavailable mounts and symlink refusal passed\n";
}
