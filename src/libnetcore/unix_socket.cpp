#include <netcore/unix_socket.h>

namespace netcore {
    auto unix_socket::apply_permissions() const -> void {
        if (mode) std::filesystem::permissions(path, mode.value());
        if (owner || group) ext::chown(path, owner, group);
    }
}
