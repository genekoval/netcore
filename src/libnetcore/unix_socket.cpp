#include <netcore/unix_socket.h>

#include <ext/except.h>
#include <ext/string.h>
#include <grp.h>
#include <memory>
#include <pwd.h>
#include <sstream>
#include <unistd.h>

namespace fs = std::filesystem;

namespace netcore {
    constexpr auto default_buffer_length = 1024;

    static auto get_uid(const std::string& username) -> uid_t {
        auto buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
        if (buflen == -1) buflen = default_buffer_length;

        auto buf = std::unique_ptr<char[]>(new char[buflen]);

        auto pwd = passwd();
        passwd* result = nullptr;

        const auto ret = getpwnam_r(
            username.c_str(),
            &pwd,
            buf.get(),
            buflen,
            &result
        );

        if (ret == 0) {
            if (result) return result->pw_uid;

            auto os = std::ostringstream();
            os << QUOTE(username) << ": no such user";
            throw std::runtime_error(os.str());
        }

        throw std::system_error(
            ret,
            std::generic_category(),
            "could not look up uid for `" + username + "`"
        );
    }

    static auto get_gid(const std::string& group_name) -> gid_t {
        auto buflen = sysconf(_SC_GETGR_R_SIZE_MAX);
        if (buflen == -1) buflen = default_buffer_length;

        auto buf = std::unique_ptr<char[]>(new char[buflen]);

        auto grp = group();
        group* result = nullptr;

        const auto ret = getgrnam_r(
            group_name.c_str(),
            &grp,
            buf.get(),
            buflen,
            &result
        );

        if (ret == 0) {
            if (result) return result->gr_gid;

            auto os = std::ostringstream();
            os << QUOTE(group_name) << ": no such group";
            throw std::runtime_error(os.str());
        }

        throw std::system_error(
            ret,
            std::generic_category(),
            "could not look up gid for `" + group_name + "`"
        );
    }

    static auto chown(
        const std::string& path,
        uid_t owner,
        gid_t group
    ) -> void {
        if (::chown(path.c_str(), owner, group) == -1) {
            auto os = std::ostringstream();
            os << "changing ownership of '" << path << "'";
            throw ext::system_error(os.str());
        }
    }

    static auto chown(
        const std::string& path,
        const owner_t& owner,
        const group_t& group
    ) -> void {
        auto o = std::visit([](auto&& arg) -> uid_t {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, uid_t>) return arg;
            else if constexpr (std::is_same_v<T, std::string>) {
                return get_uid(arg);
            }
        }, owner);

        auto g = std::visit([](auto&& arg) -> gid_t {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, gid_t>) return arg;
            else if constexpr (std::is_same_v<T, std::string>) {
                return get_gid(arg);
            }
        }, group);

        chown(path, o, g);
    }

    auto unix_socket::apply_permissions() const -> void {
        if (mode) fs::permissions(path, mode.value());
        if (owner || group) {
            chown(
                path,
                owner.value_or(getuid()),
                group.value_or(getgid())
            );
        }
    }
}
