#include "runner.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <csignal>
#include <cstring>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

// =========================================================================
// Execute a compiler phase (check or build) via fork/exec/pipe/poll
// =========================================================================
Runner::PhaseResult Runner::runPhase(const std::string& cmd, unsigned timeoutMs) const {
    PhaseResult pr;

    int stdoutPipe[2];
    int stderrPipe[2];
    if (pipe(stdoutPipe) < 0 || pipe(stderrPipe) < 0) {
        pr.stderr = "pipe() failed";
        return pr;
    }

    pid_t pid = fork();
    if (pid < 0) {
        pr.stderr = "fork() failed";
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        close(stderrPipe[0]); close(stderrPipe[1]);
        return pr;
    }

    if (pid == 0) {
        // ── Child ──────────────────────────────────────────────
        close(stdoutPipe[0]);
        close(stderrPipe[0]);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stdoutPipe[1]);
        close(stderrPipe[1]);

        execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        _exit(127);
    }

    // ── Parent ─────────────────────────────────────────────────────
    close(stdoutPipe[1]);
    close(stderrPipe[1]);

    std::string output;
    std::string errOutput;
    std::array<char, 4096> buf{};
    auto start = std::chrono::steady_clock::now();
    bool timedOut = false;
    bool stdoutDone = false, stderrDone = false;

    while (!stdoutDone || !stderrDone) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > timeoutMs) {
            timedOut = true;
            break;
        }

        struct pollfd fds[2] = {
            {stdoutPipe[0], POLLIN, 0},
            {stderrPipe[0], POLLIN, 0},
        };
        int remaining = timeoutMs - static_cast<int>(elapsed.count());
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
            if (n > 0) { buf[n] = '\0'; errOutput += buf.data(); }
            else stderrDone = true;
        } else if (fds[1].revents & POLLHUP) {
            stderrDone = true;
        }
    }

    // ── Kill child if timed out ────────────────────────────────────
    if (timedOut) {
        kill(pid, SIGTERM);
        usleep(100000);
        kill(pid, SIGKILL);
    }

    // ── Reap child ─────────────────────────────────────────────────
    int rawStatus = 0;
    waitpid(pid, &rawStatus, 0);

    close(stdoutPipe[0]);
    close(stderrPipe[0]);

    // ── Decode result ──────────────────────────────────────────────
    pr.timedOut = timedOut;
    pr.stdout = std::move(output);
    pr.stderr = std::move(errOutput);

    if (timedOut) return pr;

    if (WIFEXITED(rawStatus)) {
        pr.exitCode = WEXITSTATUS(rawStatus);
        pr.ok = (pr.exitCode == 0);
    } else if (WIFSIGNALED(rawStatus)) {
        pr.termSig = WTERMSIG(rawStatus);
        pr.ok = false;
    }

    return pr;
}

// =========================================================================
// Two-phase runner: check → build
// =========================================================================
FuzzOutcome Runner::run(std::string_view sourcePath) const {
    FuzzOutcome fo;

    // ── Phase 1: Checker ───────────────────────────────────────────
    {
        std::string cmd = lucisPath_ + " check " + std::string(sourcePath);
        auto pr = runPhase(cmd, timeoutMs_);

        fo.checkerTimedOut = pr.timedOut;
        fo.checkerExitCode = pr.exitCode;
        fo.checkerTermSig  = pr.termSig;
        fo.checkerStdout   = std::move(pr.stdout);
        fo.checkerStderr   = std::move(pr.stderr);

        // If checker failed or crashed, stop here
        if (pr.timedOut || !pr.ok || pr.termSig != 0)
            return ResultClassifier::classify(std::move(fo));
    }

    // ── Phase 2: Build (checker passed) ─────────────────────────────
    {
        std::string cmd = lucisPath_ + " build " + std::string(sourcePath);
        auto pr = runPhase(cmd, timeoutMs_);

        fo.builderTimedOut = pr.timedOut;
        fo.builderExitCode = pr.exitCode;
        fo.builderTermSig  = pr.termSig;
        fo.builderStdout   = std::move(pr.stdout);
        fo.builderStderr   = std::move(pr.stderr);
    }

    return ResultClassifier::classify(std::move(fo));
}

// =========================================================================
// Constructor
// =========================================================================
Runner::Runner(std::string lucisPath, unsigned timeoutMs)
    : lucisPath_(std::move(lucisPath)), timeoutMs_(timeoutMs) {}
