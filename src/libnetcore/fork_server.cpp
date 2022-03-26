#include <netcore/server.h>

#include <ext/except.h>
#include <sys/wait.h>
#include <timber/timber>
#include <unistd.h>

namespace netcore {
    fork_server::fork_server(const connection_handler on_connection) :
        m_server(on_connection)
    {}

    auto fork_server::start(
        const unix_socket& un,
        const std::function<void()>& callback,
        int backlog
    ) -> process_type {
        int pipefd[2];
        pipe(pipefd);

        auto ready = false;

        pid = fork();

        if (pid > 0) { // Client
            close(pipefd[1]);
            if (read(pipefd[0], &ready, sizeof(ready)) == -1) {
                throw ext::system_error(
                    "Failed to read status from server process"
                );
            }

            if (!ready) throw std::runtime_error("Failed to start server");

            TIMBER_DEBUG("Started server with PID: {}", pid);
            return process_type::client;
        }

        close(pipefd[0]);

        m_server.listen(un, [&]() {
            callback();

            ready = true;
            write(pipefd[1], &ready, sizeof(ready));
        });

        return process_type::server;
    }

    auto fork_server::stop() -> int {
        return stop(SIGINT);
    }

    auto fork_server::stop(int signal) -> int {
        auto status = 0;

        TIMBER_DEBUG("Sending signal ({}) to PID: {}", signal, pid);
        if (kill(pid, signal) == -1) throw ext::system_error(
            "Failed to kill server process"
        );

        do {
            if (waitpid(pid, &status, 0) == -1) throw ext::system_error(
                "Failed to wait for server termination"
            );
        } while (!WIFEXITED(status));

        auto exit_code = WEXITSTATUS(status);

        TIMBER_DEBUG("Server exited with code: {}", exit_code);
        return exit_code;
    }
}
