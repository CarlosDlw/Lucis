#include "runner.hpp"

#include <array>
#include <chrono>
#include <csignal>
#include <cstring>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

FuzzOutcome Runner::run(std::string_view sourcePath) const {
    std::string cmd = lucisPath_ + " build " + std::string(sourcePath);

    // ── Fork + exec + pipe ────────────────────────────────────────
    int stdoutPipe[2];
    int stderrPipe[2];
    if (pipe(stdoutPipe) < 0 || pipe(stderrPipe) < 0)
        return {FuzzResult::BuildError, -1, "pipe() failed", ""};

    pid_t pid = fork();
    if (pid < 0)
        return {FuzzResult::BuildError, -1, "fork() failed", ""};

    if (pid == 0) {
        // ── Child ──────────────────────────────────────────────
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        // Run via sh -c so shell expansion works
        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(127);
    }

    // ── Parent ─────────────────────────────────────────────────────
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    std::string output;
    std::array<char, 4096> buf{};
    auto start = std::chrono::steady_clock::now();
    bool timedOut = false;
    bool stdoutDone = false, stderrDone = false;

    while (!stdoutDone || !stderrDone) {
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > timeoutMs_) {
            timedOut = true;
            break;
        }

        struct pollfd fds[2] = {
            {stdoutPipe[0], POLLIN, 0},
            {stderrPipe[0], POLLIN, 0},
        };
        int remaining = timeoutMs_ - static_cast<int>(elapsed.count());
        int ret = poll(fds, 2, std::max(remaining, 10));

        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (fds[0].revents & POLLIN) {
            ssize_t n = read(stdoutPipe[0], buf.data(), buf.size() - 1);
            if (n > 0) { buf[n] = '\0'; output += buf.data(); }
            else stdoutDone = true;
        } else if (fds[0].revents & POLLHUP) {
            stdoutDone = true;
        }

        if (fds[1].revents & POLLIN) {
            ssize_t n = read(stderrPipe[0], buf.data(), buf.size() - 1);
            if (n > 0) { buf[n] = '\0'; output += buf.data(); }
            else stderrDone = true;
        } else if (fds[1].revents & POLLHUP) {
            stderrDone = true;
        }
    }

    // ── Kill child if timed out ────────────────────────────────────
    if (timedOut) {
        kill(pid, SIGTERM);
        // Give it a moment to die, then force
        usleep(100000);
        kill(pid, SIGKILL);
    }

    // ── Reap child ─────────────────────────────────────────────────
    int rawStatus = 0;
    waitpid(pid, &rawStatus, 0);

    close(stdoutPipe[0]);
    close(stderrPipe[0]);

    // ── Decode result ──────────────────────────────────────────────
    if (timedOut)
        return {FuzzResult::Timeout, SignalTimeout,
                "timed out after " + std::to_string(timeoutMs_) + "ms", output};

    int exitCode = -1;
    bool signaled = false;
    int termSig = 0;

    if (WIFEXITED(rawStatus)) {
        exitCode = WEXITSTATUS(rawStatus);
    } else if (WIFSIGNALED(rawStatus)) {
        termSig = WTERMSIG(rawStatus);
        signaled = true;
        exitCode = termSig;
    }

    if (signaled) {
        // SIGTERM/SIGKILL from our timeout → already handled above.
        // Real crashes are signals like SIGSEGV, SIGABRT, SIGBUS, SIGILL.
        static const int crashSignals[] = {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL};
        bool isCrash = false;
        for (int s : crashSignals) {
            if (termSig == s) { isCrash = true; break; }
        }
        FuzzResult kind = isCrash ? FuzzResult::Crash : FuzzResult::BuildError;
        return {kind, SignalCrash, output, ""};
    }

    FuzzOutcome o;
    o.exitCode = exitCode;
    o.stderr = output;
    o.kind = FuzzOutcome::classify(output, exitCode);
    return o;
}

Runner::Runner(std::string lucisPath, unsigned timeoutMs)
    : lucisPath_(std::move(lucisPath)), timeoutMs_(timeoutMs) {}
