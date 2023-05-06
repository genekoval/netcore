#include <netcore/proc/command.hpp>

#include <timber/timber>

namespace fs = std::filesystem;

namespace netcore::proc {
    arguments::~arguments() {
        for (auto* string : storage) if (string) delete[] string;
    }

    auto arguments::operator[](std::size_t index) -> char* {
        return storage[index];
    }

    auto arguments::append(std::string_view arg) -> void {
        auto* string = storage.emplace_back(new char[arg.size() + 1]);
        arg.copy(string, arg.size());
        string[arg.size()] = '\0';
    }

    auto arguments::append_null() -> void {
        storage.push_back(nullptr);
    }

    auto arguments::get() noexcept -> char** {
        return storage.data();
    }

    auto arguments::size() const noexcept -> std::size_t {
        return storage.size();
    }

    command::command(std::string_view program) {
        arg(program);
    }

    auto command::err(proc::stdio stdio) -> command& {
        this->stdio[2] = stdio;
        return *this;
    }

    [[noreturn]]
    auto command::exec() noexcept -> void {
        try {
            if (directory) std::filesystem::current_path(*directory);
        }
        catch (const std::system_error& ex) {
            fmt::print(
                stderr,
                "Failed to spawn '{}': {}\n",
                argv[0],
                ex.what()
            );

            exit(ex.code().value());
        }

        argv.append_null();
        execvp(argv[0], argv.get());

        fmt::print(stderr, "{}: {}\n", argv[0], std::strerror(errno));
        exit(errno);
    }

    auto command::arg(std::string_view arg) -> command& {
        argv.append(arg);
        return *this;
    }

    auto command::in(proc::stdio stdio) -> command& {
        this->stdio[0] = stdio;
        return *this;
    }

    auto command::out(proc::stdio stdio) -> command& {
        this->stdio[1] = stdio;
        return *this;
    }

    auto command::spawn() -> child {
        auto streams = stdio_streams {
            stdio_stream(0, stdio[0]),
            stdio_stream(1, stdio[1]),
            stdio_stream(2, stdio[2])
        };

        auto child = fork();

        if (child) {
            TIMBER_DEBUG("Spawn child {}: {}", child, argv);

            for (auto& stream : streams) stream.parent();
            return proc::child(std::move(child), std::move(streams));
        }

        for (auto& stream : streams) stream.child();
        exec();
    }

    auto command::working_directory(const fs::path& path) -> command& {
        directory = path;
        return *this;
    }
}
