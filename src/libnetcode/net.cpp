#include <nova/net.h>

#include <nova/ext/except.h>
#include <nova/logger.h>

#include <csignal>
#include <fcntl.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

namespace fs = std::filesystem;

using nova::net::event_monitor;
using nova::net::server;
using nova::net::signalfd;
using nova::net::socket;
using nova::net::sock_ptr;
using std::function;
using std::make_shared;
using std::make_unique;
using std::string;
using std::vector;
using nova::ext::except::system_error;

constexpr size_t RECV_BUFFER_SIZE = 256;
constexpr int EPOLL_WAIT_TIMEOUT = -1;

event_monitor::event_monitor(const int max_event_count) :
    epoll_sock(epoll_create1(0)),
    events(make_unique<epoll_event[]>(max_event_count)),
    max_events(max_event_count)
{
    if (!epoll_sock.valid()) throw system_error("epoll create failure");
    DEBUG() << "Created event monitor(fd: " << epoll_sock.fd()
        << ", max events: " << max_event_count << ")";
}

void event_monitor::add(int fd) {
    monitor.data.fd = fd;
    if (epoll_ctl(epoll_sock.fd(), EPOLL_CTL_ADD, fd, &monitor) == -1)
        throw system_error("event monitor failed to add socket");
    DEBUG() << "Event monitor tracking fd: " << fd;
}

uint32_t event_monitor::get_types() { return monitor.events; }

void event_monitor::set_types(const uint32_t event_types) {
    monitor.events = event_types;
}

void event_monitor::wait(const function<void(int)>& event_handler) {
    const auto ready = epoll_wait(
        epoll_sock.fd(),
        events.get(),
        max_events,
        EPOLL_WAIT_TIMEOUT
    );

    if (ready == -1) throw system_error("epoll wait failure");

    for (int i = 0; i < ready; i++)
        if (events[i].events & EPOLLRDHUP)
            WARN() << "Event Monitor: EPOLLRDHUP flag detected: "
                "client disconnected";
        else
            event_handler(events[i].data.fd);
}

server::server(function<void(sock_ptr)> connection_handler) :
    connection_handler(connection_handler)
{}

int server::accept(int fd) {
    auto client_addr = sockaddr_storage();
    socklen_t addrlen = sizeof client_addr;

    auto client = ::accept(fd, (sockaddr*) &client_addr, &addrlen);

    if (client == -1)
        throw system_error("could not accept client connection");

    int client_flags = fcntl(client, F_GETFL, 0);
    if (client_flags == -1)
        throw system_error("could not get flags");

    if (fcntl(client, F_SETFL, client_flags | O_NONBLOCK) == -1)
        throw system_error("could not set flags");

    return client;
}

void server::listen(const socket& sock, int backlog) const {
    if (::listen(sock.fd(), backlog) == -1)
        throw system_error("socket listen failure");

    DEBUG() << "Socket(fd: " << sock.fd() << ", backlog: " << backlog
        << ") listening for connections...";

    // Leave room for full backlog of clients, server socket, and signal socket.
    event_monitor emonit(backlog + 2);

    // Event types for server socket.
    emonit.set_types(EPOLLIN);
    emonit.add(sock.fd());

    const auto sigfd = signalfd::create({SIGINT, SIGTERM});
    // Event types for client sockets.
    emonit.set_types(EPOLLIN | EPOLLET | EPOLLRDHUP);
    emonit.add(sigfd.fd());

    int signal_status = 0;
    while (!signal_status) emonit.wait([&](int current_fd) {
        if (current_fd == sock.fd()) emonit.add(accept(sock.fd()));
        else if (current_fd == sigfd.fd()) signal_status = sigfd.status();
        else connection_handler(make_shared<socket>(current_fd));
    });
}

void server::listen(const fs::path& path, int backlog) const {
    listen(path, fs::perms::unknown, backlog);
}

void server::listen(
    const fs::path& path,
    const fs::perms perms,
    int backlog
) const {
    const socket sock(::socket(AF_UNIX, SOCK_STREAM, 0));
    if (!sock.valid()) throw system_error("could not create server socket");

    DEBUG() << "Created socket(fd: " << sock.fd() << ")";

    auto serv_addr = sockaddr_un();
    serv_addr.sun_family = AF_UNIX;

    auto path_string = path.string();
    path_string.copy(serv_addr.sun_path, path_string.size(), 0);

    // Remove the socket file if it exists.
    if (fs::remove(path)) { WARN() << "Removed socket path: " << path; }

    if (bind(sock.fd(), (sockaddr*) &serv_addr, sizeof(sockaddr_un)) == -1)
        throw system_error("could not bind " + path_string);

    DEBUG() << "Bound Unix socket(fd: " << sock.fd() << ") to path: " << path;

    if (perms != fs::perms::unknown) fs::permissions(path, perms);

    listen(sock, backlog);
    fs::remove(path);
}

signalfd::signalfd(int fd) : sock(fd) {}

nova::net::signalfd signalfd::create(const vector<int>& signals) {
    sigset_t mask;
    sigemptyset(&mask);

    for (const auto& sig: signals) sigaddset(&mask, sig);

    if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) throw system_error("sigprocmask failure");

    signalfd sfd(::signalfd(-1, &mask, 0));
    if (!sfd.sock.valid()) throw system_error("signalfd system call failure");

    DEBUG() << "Created signalfd(fd: " << sfd.fd() << ")";

    return sfd;
}

int signalfd::fd() const { return sock.fd(); }

uint32_t signalfd::status() const {
    signalfd_siginfo fdsi;
    const auto infolen = sizeof(signalfd_siginfo);

    const auto byte_count = read(sock.fd(), &fdsi, infolen);
    if (byte_count != infolen)
        throw system_error("could not read signal socket status");

    return fdsi.ssi_signo;
}

socket::socket(int fd) : file_descriptor(fd) {}

socket::~socket() { close(); }

socket::socket(socket&& other) noexcept :
    file_descriptor(std::exchange(other.file_descriptor, -1))
{}

nova::net::socket& socket::operator=(socket other) noexcept {
    std::swap(file_descriptor, other.file_descriptor);
    return *this;
}

void socket::close() {
    if (!valid()) return;

    if (::close(file_descriptor) == -1)
        ERROR() << "Failed to close socket(fd: " << file_descriptor << ")";
    else
        DEBUG() << "Closed socket(fd: " << file_descriptor << ")";

    file_descriptor = -1;
}

int socket::fd() const { return file_descriptor; }

string socket::fd_error_text() const {
    return "socket(" + std::to_string(file_descriptor) + "): ";
}

string socket::receive() const {
    auto data = string();
    char buffer[RECV_BUFFER_SIZE];
    auto byte_count = 0;

    do {
        byte_count = recv(fd(), buffer, RECV_BUFFER_SIZE, 0);
        if (byte_count > 0) data.append(buffer, byte_count);
    } while (byte_count > 0);

    if (byte_count == -1 && errno != EAGAIN)
        throw system_error(fd_error_text() + "could not read");

    return data;
}

void socket::send(const string& data) const {
    if (::send(fd(), data.c_str(), data.size(), MSG_NOSIGNAL) == -1)
        throw system_error(fd_error_text() + "could not send");
}

bool socket::valid() const { return file_descriptor >= 0; }
