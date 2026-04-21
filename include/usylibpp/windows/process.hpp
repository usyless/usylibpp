#pragma once

#include "../types.hpp"

#ifdef USYLIBPP_ENABLE_WIL
#include "../aliases.hpp" // IWYU pragma: export
#include <string>
#include <thread>
#include <functional>
#include <filesystem>
#include <windows.h>
#include <atomic>
#include <mutex>
#include <optional>
#include <memory>
#include <shellapi.h>
#include "char_t.hpp"

#include <wil/resource.h>
#include <wil/com.h>
#endif

#ifdef USYLIBPP_ENABLE_LINUX
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <concepts>
#include <cstdint>
#include <mutex>
#include <optional>
#include <memory>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif
#endif

static_assert(true, "");
#pragma push_macro("IS_NOOP")
#undef IS_NOOP
#define IS_NOOP(func) (std::is_same_v<std::remove_cvref_t<decltype(std::declval<settings>().func)>, types::noop_t>)

namespace usylibpp::windows::process {
    struct process_output {
        int status = -1;
        std::string stdout_{};
        std::string stderr_{};
    };

    struct one_shot_process_output {
        int status = -1;
    #ifdef USYLIBPP_ENABLE_WIL
        wil::unique_handle hJob;
    #endif
    #ifdef USYLIBPP_ENABLE_LINUX
        pid_t pid = -1;
    #endif
    };

    struct async_process_output {
        struct state {
            std::atomic_bool finished{false};
            std::atomic_int status{-1};

            std::mutex out_mtx;
            std::string stdout_;
            std::string stderr_;

        #ifdef USYLIBPP_ENABLE_WIL
            wil::unique_handle process;
            wil::unique_handle job;
        #endif
        #ifdef USYLIBPP_ENABLE_LINUX
            pid_t pid{-1};
            pid_t pgid{-1};
        #endif

            std::jthread stdout_thread;
            std::jthread stderr_thread;
            std::jthread stdin_thread;
            std::jthread waiter_thread;
        };

        std::shared_ptr<state> s{};

        bool valid() const { return !!s; }

        void cancel() const {
            if (!s) return;
        #ifdef USYLIBPP_ENABLE_WIL
            if (s->job.is_valid()) {
                TerminateJobObject(s->job.get(), 1);
            } else if (s->process.is_valid()) {
                TerminateProcess(s->process.get(), 1);
            }
        #endif
        #ifdef USYLIBPP_ENABLE_LINUX
            if (s->pgid > 0) {
                ::kill(-s->pgid, SIGTERM);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                ::kill(-s->pgid, SIGKILL);
            } else if (s->pid > 0) {
                ::kill(s->pid, SIGTERM);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                ::kill(s->pid, SIGKILL);
            }
        #endif
        }

        bool is_finished() const {
            return s && s->finished.load(std::memory_order_acquire);
        }

        int status() const {
            return s ? s->status.load(std::memory_order_acquire) : -1;
        }

        // joins waiter and IO threads if needed
        process_output wait() const {
            process_output out{};
            if (!s) return out;

            if (s->waiter_thread.joinable()) s->waiter_thread.join();
            if (s->stdin_thread.joinable())  s->stdin_thread.request_stop(),  s->stdin_thread.join();
            if (s->stdout_thread.joinable()) s->stdout_thread.request_stop(), s->stdout_thread.join();
            if (s->stderr_thread.joinable()) s->stderr_thread.request_stop(), s->stderr_thread.join();

            out.status = s->status.load(std::memory_order_acquire);
            {
                std::scoped_lock lk(s->out_mtx);
                out.stdout_ = s->stdout_;
                out.stderr_ = s->stderr_;
            }
            return out;
        }

        std::optional<process_output> try_get() const {
            if (!is_finished()) return std::nullopt;
            return wait();
        }
    };

    struct process_options {
        bool allow_visible_windows = true;
        bool capture_stdout = true;
        bool capture_stderr = true;
        bool set_lifetime_of_subprocess_to_this_process = true;
        bool on_stdout_line_call_on_lines = true; // otherwise only call on buffer full
        bool on_stderr_line_call_on_lines = true;

        bool one_shot_process = false; // no stdout or stderr
        bool async = false; // incompatible with one_shot_process, allows for async process input and process killing
    };

    template <typename F1 = types::noop_t, typename F2 = types::noop_t, typename F3 = types::noop_t>
    requires (std::invocable<F1&, std::string_view> && std::invocable<F2&, std::string_view> && std::invocable<F3&, int>)
    struct process_settings {
        #ifdef USYLIBPP_ENABLE_WIL
        std::wstring_view commandline;
        #endif
        #ifdef USYLIBPP_ENABLE_LINUX
        std::string_view commandline;
        #endif
        std::string_view input = "";
        std::filesystem::path* working_directory = nullptr;

        F1 on_stdout_line{};
        F2 on_stderr_line{};

        #ifdef USYLIBPP_ENABLE_WIL
        DWORD wait_for_ms = INFINITE;
        #endif
        #ifdef USYLIBPP_ENABLE_LINUX
        uint32_t wait_for_ms = 0xFFFFFFFFu; // INFINITE-like
        #endif

        F3 async_then{}; // run once async process exits, invoked with int status
    };

    template <typename>
    struct is_process_settings : std::false_type {};

    template <typename F1, typename F2, typename F3>
    struct is_process_settings<process_settings<F1, F2, F3>> : std::true_type {};

    template <typename T>
    concept process_settings_type = is_process_settings<std::remove_cvref_t<T>>::value;

    template <process_options opts = {}>
    using process_return_t = std::conditional_t<
        opts.one_shot_process,
        one_shot_process_output,
        std::conditional_t<opts.async, async_process_output, process_output>
    >;
}

// only true if windows
// linux at the end of the file
#ifdef USYLIBPP_ENABLE_WIL
namespace usylibpp::windows::process {
    namespace internal {
        /**
         * The callback is a function which takes one argument of std::string_view
         */
        template <bool with_output = true, bool break_line = true, typename Callback = types::noop_t>
        requires (std::invocable<Callback&, std::string_view>)
        inline std::string read_from_pipe(std::stop_token stop, HANDLE pipe, Callback&& on_line = {}) {
            std::string output;
            char buffer[4096];
            DWORD bytesRead{0};

            std::string partialLine;

            HANDLE threadHandle{nullptr};
            if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &threadHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
                std::stop_callback cb(stop, [&pipe, threadHandle]() {
                    CancelSynchronousIo(threadHandle);
                });
            }

            while (ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                if (stop.stop_requested()) break;

                if constexpr (with_output) output.append(buffer, bytesRead);

                if constexpr (!std::is_same_v<Callback, types::noop_t>) {
                    if constexpr (!break_line) {
                        std::invoke(on_line, std::string_view{buffer, bytesRead});
                    } else {
                        partialLine.append(buffer, bytesRead);

                        std::size_t start_pos = 0;
                        std::size_t new_line_pos = 0;
                        while ((new_line_pos = partialLine.find('\n', new_line_pos)) != std::string::npos) {
                            std::size_t line_end = new_line_pos;

                            if (line_end > start_pos && partialLine[line_end - 1] == '\r') --line_end;

                            std::invoke(on_line, std::string_view{partialLine.data() + start_pos, line_end - start_pos});
                            start_pos = ++new_line_pos;
                        }

                        partialLine.erase(0, start_pos);
                    }
                }
            }

            if constexpr (!std::is_same_v<Callback, types::noop_t> && break_line) {
                if (!partialLine.empty()) std::invoke(on_line, partialLine);
            }

            return output;
        }
    }

    /**
        * Run a process and either capture its output or dont
        * Blocks until the process exits
        */
    template <process_options opts = {}, process_settings_type settings>
    inline process_return_t<opts> run_process(settings&& options) {
        if constexpr (opts.one_shot_process && opts.async) {
            static_assert(!std::is_same_v<process_options, process_options>, "Async one-shot processes are not supported.");
        }
        static constexpr auto ASYNC = opts.async && !opts.one_shot_process;
        static constexpr auto ONESHOT = opts.one_shot_process && !opts.async;
        static constexpr auto NORMAL = !ASYNC && !ONESHOT;

        if constexpr (!ASYNC && !IS_NOOP(async_then)) {
            static_assert(!std::is_same_v<process_options, process_options>, "async_then is not usable without async!");
        }

        if (options.commandline.empty()) {
            return {};
        }
        
        SECURITY_ATTRIBUTES saAttr{};
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        wil::unique_handle hStdOutRead = NULL, hStdOutWrite = NULL;
        wil::unique_handle hStdErrRead = NULL, hStdErrWrite = NULL;
        wil::unique_handle hStdInRead = NULL, hStdInWrite = NULL;

        if constexpr (!opts.one_shot_process) {
            HANDLE hReadOut = NULL, hWriteOut = NULL;
            HANDLE hReadErr = NULL, hWriteErr = NULL;
            HANDLE hInRead = NULL, hInWrite = NULL;
            
            if constexpr (opts.capture_stdout || !IS_NOOP(on_stdout_line)) {
                if (!CreatePipe(&hReadOut, &hWriteOut, &saAttr, 0)) {
                    return {};
                }
            }

            hStdOutRead.reset(hReadOut);
            hStdOutWrite.reset(hWriteOut);

            if constexpr (opts.capture_stdout || !IS_NOOP(on_stdout_line)) {
                if (!SetHandleInformation(hStdOutRead.get(), HANDLE_FLAG_INHERIT, 0)) {
                    return {};
                }
            }

            if constexpr (opts.capture_stderr || !IS_NOOP(on_stderr_line)) {
                if (!CreatePipe(&hReadErr, &hWriteErr, &saAttr, 0)) {
                    return {};
                }
            }

            hStdErrRead.reset(hReadErr);
            hStdErrWrite.reset(hWriteErr);

            if constexpr (opts.capture_stderr || !IS_NOOP(on_stderr_line)) {
                if (!SetHandleInformation(hStdErrRead.get(), HANDLE_FLAG_INHERIT, 0)) {
                    return {};
                }
            }

            if (!options.input.empty()) {
                if (!CreatePipe(&hInRead, &hInWrite, &saAttr, 0)) {
                    return {};
                }
                
                hStdInRead.reset(hInRead);
                hStdInWrite.reset(hInWrite);

                if (!SetHandleInformation(hStdInWrite.get(), HANDLE_FLAG_INHERIT, 0)) {
                    return {};
                }
            }
        }

        STARTUPINFO si{};
        si.cb = sizeof(STARTUPINFO);
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        si.wShowWindow = (opts.allow_visible_windows) ? SW_SHOW : SW_HIDE;
        si.hStdOutput = hStdOutWrite.get();
        si.hStdError = hStdErrWrite.get();
        si.hStdInput = hStdInRead.get();

        PROCESS_INFORMATION pi{};

        std::wstring cmdline{options.commandline};
        BOOL success = CreateProcessW(
            NULL,
            cmdline.data(),
            NULL,
            NULL,
            TRUE,
            (opts.allow_visible_windows) ? NULL : CREATE_NO_WINDOW,
            NULL,
            (options.working_directory && !options.working_directory->empty()) ? options.working_directory->c_str() : NULL,
            &si,
            &pi
        );

        hStdOutWrite.reset();
        hStdErrWrite.reset();
        hStdInRead.reset();

        if (!success) {
            return {};
        }

        wil::unique_handle process{pi.hProcess};
        wil::unique_handle thread{pi.hThread};

        wil::unique_handle hJob;

        if constexpr (opts.set_lifetime_of_subprocess_to_this_process) {
            hJob.reset(CreateJobObjectW(NULL, NULL));
            if (hJob.is_valid()) {
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
                jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;

                if (SetInformationJobObject(
                        hJob.get(),
                        JobObjectExtendedLimitInformation,
                        &jeli,
                        sizeof(jeli))) {
                    AssignProcessToJobObject(hJob.get(), process.get());
                }
            }
        }

        if constexpr (ASYNC) {
            async_process_output out;
            out.s = std::make_shared<async_process_output::state>();
            auto st = out.s;

            st->process = std::move(process);
            st->job = std::move(hJob);

            if constexpr (opts.capture_stdout || !IS_NOOP(on_stdout_line)) {
                st->stdout_thread = std::jthread([st, h = std::move(hStdOutRead), &options](std::stop_token tok) {
                    auto txt = internal::read_from_pipe<opts.capture_stdout, opts.on_stdout_line_call_on_lines>(tok, h.get(), options.on_stdout_line);
                    if constexpr (opts.capture_stdout) {
                        std::scoped_lock lk(st->out_mtx);
                        st->stdout_ = std::move(txt);
                    }
                });
            }

            if constexpr (opts.capture_stderr || !IS_NOOP(on_stderr_line)) {
                st->stderr_thread = std::jthread([st, h = std::move(hStdErrRead), &options](std::stop_token tok) {
                    auto txt = internal::read_from_pipe<opts.capture_stderr, opts.on_stderr_line_call_on_lines>(tok, h.get(), options.on_stderr_line);
                    if constexpr (opts.capture_stderr) {
                        std::scoped_lock lk(st->out_mtx);
                        st->stderr_ = std::move(txt);
                    }
                });
            }

            if (!options.input.empty() && hStdInWrite.is_valid()) {
                st->stdin_thread = std::jthread([hIn = std::move(hStdInWrite), &options](std::stop_token tok) mutable {
                    const char* p = options.input.data();
                    size_t rem = options.input.size();
                    while (rem > 0 && !tok.stop_requested()) {
                        DWORD chunk = static_cast<DWORD>(std::min<size_t>(rem, 64 * 1024));
                        DWORD written = 0;
                        if (!WriteFile(hIn.get(), p, chunk, &written, NULL) || written == 0) break;
                        p += written; rem -= written;
                    }
                    hIn.reset();
                });
            }

            st->waiter_thread = std::jthread([st, wait_ms = options.wait_for_ms, async_then = std::forward<decltype(options.async_then)>(options.async_then)](std::stop_token) {
                DWORD wr = WaitForSingleObject(st->process.get(), wait_ms);
                if (wr == WAIT_TIMEOUT) {
                    if (st->job.is_valid()) TerminateJobObject(st->job.get(), 1);
                    else TerminateProcess(st->process.get(), 1);
                    WaitForSingleObject(st->process.get(), INFINITE);
                }

                DWORD ec = 0;
                if (GetExitCodeProcess(st->process.get(), &ec)) st->status.store(static_cast<int>(ec), std::memory_order_release);
                else st->status.store(-1, std::memory_order_release);

                st->finished.store(true, std::memory_order_release);
                if constexpr (!IS_NOOP(async_then)) {
                    std::invoke(async_then, static_cast<int>(ec));
                }
            });
            return out;
        }

        if constexpr (ONESHOT) {
            return one_shot_process_output{ 0, std::move(hJob) };
        }

        if constexpr (NORMAL) {
            std::string stdoutOutput;
            std::string stderrOutput;

            std::jthread stdoutThread;
            if constexpr (opts.capture_stdout || !IS_NOOP(on_stdout_line)) {
                stdoutThread = std::jthread([&stdoutOutput, hStdOutRead = hStdOutRead.get(), &options](std::stop_token st) {
                    stdoutOutput = internal::read_from_pipe<opts.capture_stdout, opts.on_stdout_line_call_on_lines>(st, hStdOutRead, options.on_stdout_line);
                });
            }

            std::jthread stderrThread;
            if constexpr (opts.capture_stderr || !IS_NOOP(on_stderr_line)) {
                stderrThread = std::jthread([&stderrOutput, hStdErrRead = hStdErrRead.get(), &options](std::stop_token st) {
                    stderrOutput = internal::read_from_pipe<opts.capture_stderr, opts.on_stderr_line_call_on_lines>(st, hStdErrRead, options.on_stderr_line);
                });
            }

            std::jthread stdinThread;
            if (!options.input.empty() && hStdInWrite.is_valid()) {
                stdinThread = std::jthread([&hStdInWrite, &options](std::stop_token st) {
                    const auto h = hStdInWrite.get();
                    const char* p = options.input.data();
                    size_t remaining = options.input.size();

                    while (remaining > 0 && !st.stop_requested()) {
                        DWORD chunk = static_cast<DWORD>(std::min<size_t>(remaining, 64 * 1024));
                        DWORD written = 0;
                        if (!WriteFile(h, p, chunk, &written, NULL) || written == 0) {
                            break;
                        }
                        p += written;
                        remaining -= written;
                    }

                    hStdInWrite.reset();
                });
            } else {
                hStdInWrite.reset();
            }

            DWORD result = WaitForSingleObject(process.get(), options.wait_for_ms);

            if (result == WAIT_TIMEOUT) { 
                TerminateProcess(process.get(), 1); 
                WaitForSingleObject(process.get(), INFINITE);

                if (stdinThread.joinable()) stdinThread.request_stop();
                if (stdoutThread.joinable()) stdoutThread.request_stop();
                if (stderrThread.joinable()) stderrThread.request_stop();
            }

            DWORD exitCode = 0;
            GetExitCodeProcess(process.get(), &exitCode);

            return process_output{ static_cast<int>(exitCode), stdoutOutput, stderrOutput };
        }
    }

    struct admin_process_settings {
        const std::basic_string<WIN_CHAR>* filename;
        const std::basic_string<WIN_CHAR>* args{nullptr};

        const std::basic_string<WIN_CHAR>* working_directory{nullptr};
    };

    struct admin_process_options {
        bool allow_visible_windows = true;
    };

    struct admin_process_output {
        enum class Status {
            Success,

            NoFilename,
            UACRejected,
            OtherError
        };
        Status status = Status::UACRejected;
    };

    /**
     * This will ignore most options passed in
     */
    template <admin_process_options opts = {}>
    inline admin_process_output run_admin_process(const admin_process_settings& options) {
        if (!options.filename || options.filename->empty()) {
            return { admin_process_output::Status::NoFilename };
        }

        SHELLEXECUTEINFO sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask  = SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd   = nullptr;
        #ifdef UNICODE
        sei.lpVerb = L"runas";
        #else
        sei.lpVerb = "runas";
        #endif
        sei.lpFile = options.filename->c_str();
        sei.lpParameters = (!options.args || options.args->empty()) ? nullptr : options.args->c_str();
        sei.lpDirectory = (!options.working_directory || options.working_directory->empty()) ? nullptr : options.working_directory->c_str();
        sei.nShow = (opts.allow_visible_windows) ? SW_SHOW : SW_HIDE;

        if (!ShellExecuteEx(&sei)) {
            if (GetLastError() == ERROR_CANCELLED) {
                return { admin_process_output::Status::UACRejected };
            }
            return { admin_process_output::Status::OtherError };
        }
        wil::unique_handle hProcess{sei.hProcess};
        return { admin_process_output::Status::Success };
    }
}
#endif

#ifdef USYLIBPP_ENABLE_LINUX

namespace usylibpp::windows::process {
    namespace internal {
        template <bool with_output = true, bool break_line = true, typename Callback = types::noop_t>
        requires (std::invocable<Callback&, std::string_view>)
        inline std::string read_from_fd(std::stop_token stop, int fd, Callback&& on_line = {}) {
            std::string output;
            char buffer[4096];
            std::string partialLine;

            while (!stop.stop_requested()) {
                ssize_t n = ::read(fd, buffer, sizeof(buffer));
                if (n == 0) break;                 // EOF
                if (n < 0) {
                    if (errno == EINTR) continue;  // interrupted syscall
                    break;
                }

                if constexpr (with_output) output.append(buffer, static_cast<size_t>(n));

                if constexpr (!std::is_same_v<std::remove_cvref_t<Callback>, types::noop_t>) {
                    if constexpr (!break_line) {
                        std::invoke(on_line, std::string_view{buffer, static_cast<size_t>(n)});
                    } else {
                        partialLine.append(buffer, static_cast<size_t>(n));

                        size_t start = 0;
                        size_t pos = 0;
                        while ((pos = partialLine.find('\n', pos)) != std::string::npos) {
                            size_t end = pos;
                            if (end > start && partialLine[end - 1] == '\r') --end;

                            std::invoke(on_line, std::string_view{partialLine.data() + start, end - start});
                            start = ++pos;
                        }
                        partialLine.erase(0, start);
                    }
                }
            }

            if constexpr (!std::is_same_v<std::remove_cvref_t<Callback>, types::noop_t> && break_line) {
                if (!partialLine.empty()) std::invoke(on_line, std::string_view{partialLine});
            }

            return output;
        }

        inline int set_cloexec(int fd) {
            int flags = ::fcntl(fd, F_GETFD);
            if (flags < 0) return -1;
            if (::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0) return -1;
            return 0;
        }

        inline int make_pipe_cloexec(int fds[2]) {
#if defined(__linux__)
            if (::pipe2(fds, O_CLOEXEC) == 0) return 0;
            if (errno != ENOSYS) return -1;
#endif
            if (::pipe(fds) != 0) return -1;
            if (set_cloexec(fds[0]) != 0 || set_cloexec(fds[1]) != 0) {
                ::close(fds[0]);
                ::close(fds[1]);
                fds[0] = fds[1] = -1;
                return -1;
            }
            return 0;
        }

        inline void close_if_valid(int fd) {
            if (fd != -1) ::close(fd);
        }

        inline void close_pipe(int p[2]) {
            close_if_valid(p[0]);
            close_if_valid(p[1]);
            p[0] = p[1] = -1;
        }
    }

    template <process_options opts = {}, process_settings_type settings>
    inline process_return_t<opts> run_process(settings&& options) {
        if constexpr (opts.one_shot_process && opts.async) {
            static_assert(!std::is_same_v<process_options, process_options>, "Async one-shot processes are not supported.");
        }

        static constexpr auto ASYNC = opts.async && !opts.one_shot_process;
        static constexpr auto ONESHOT = opts.one_shot_process && !opts.async;
        static constexpr auto NORMAL = !ASYNC && !ONESHOT;

        if constexpr (!ASYNC && !IS_NOOP(async_then)) {
            static_assert(!std::is_same_v<process_options, process_options>, "async_then is not usable without async!");
        }

        if (options.commandline.empty()) return {};

        static constexpr bool need_stdout = opts.capture_stdout || !IS_NOOP(on_stdout_line);
        static constexpr bool need_stderr = opts.capture_stderr || !IS_NOOP(on_stderr_line);
        static constexpr bool has_async_next = !IS_NOOP(async_then)
        const bool need_stdin = !options.input.empty();

        int out_pipe[2]{-1, -1};
        int err_pipe[2]{-1, -1};
        int in_pipe[2]{-1, -1};

        if constexpr (!opts.one_shot_process) {
            if constexpr (need_stdout) {
                if (internal::make_pipe_cloexec(out_pipe) != 0) return { -1 };
            }
            if constexpr (need_stderr) {
                if (internal::make_pipe_cloexec(err_pipe) != 0) {
                    internal::close_pipe(out_pipe);
                    return {};
                }
            }
            if (need_stdin) {
                if (internal::make_pipe_cloexec(in_pipe) != 0) {
                    internal::close_pipe(out_pipe);
                    internal::close_pipe(err_pipe);
                    return {};
                }
            }
        }

        pid_t pid = ::fork();
        if (pid < 0) {
            internal::close_pipe(out_pipe);
            internal::close_pipe(err_pipe);
            internal::close_pipe(in_pipe);
            return {};
        }

        if (pid == 0) {
            // Child
            if constexpr (!opts.one_shot_process) {
                if constexpr (need_stdout) {
                    if (::dup2(out_pipe[1], STDOUT_FILENO) < 0) _exit(127);
                }
                if constexpr (need_stderr) {
                    if (::dup2(err_pipe[1], STDERR_FILENO) < 0) _exit(127);
                }
                if (need_stdin) {
                    if (::dup2(in_pipe[0], STDIN_FILENO) < 0) _exit(127);
                }
            }

            internal::close_pipe(out_pipe);
            internal::close_pipe(err_pipe);
            internal::close_pipe(in_pipe);

            if (options.working_directory && !options.working_directory->empty()) {
                (void)::chdir(options.working_directory->c_str());
            }

#if defined(__linux__)
            if constexpr (opts.async && opts.set_lifetime_of_subprocess_to_this_process) {
                (void)::setpgid(0, 0); // child becomes its own PG leader
            } else if constexpr (opts.set_lifetime_of_subprocess_to_this_process) {
                (void)::prctl(PR_SET_PDEATHSIG, SIGKILL);
                // Close race: parent may have died before prctl call.
                if (::getppid() == 1) _exit(127);
            }
#endif

            std::string cmd{options.commandline};
            ::execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
            _exit(127);
        }

        pid_t pgid = -1;
        if constexpr (opts.async && opts.set_lifetime_of_subprocess_to_this_process) {
            (void)::setpgid(pid, pid); // race-safe best effort
            pgid = pid;
        }

        // Parent
        if constexpr (!opts.one_shot_process) {
            internal::close_if_valid(out_pipe[1]); out_pipe[1] = -1;
            internal::close_if_valid(err_pipe[1]); err_pipe[1] = -1;
            internal::close_if_valid(in_pipe[0]);  in_pipe[0]  = -1;
        }

        if constexpr (ASYNC) {
            async_process_output out;
            out.s = std::make_shared<async_process_output::state>();
            auto st = out.s;
            st->pid = pid;
            st->pgid = pgid;

            if constexpr (need_stdout) {
                st->stdout_thread = std::jthread([st, fd = out_pipe[0], &options](std::stop_token tok) mutable {
                    auto txt = internal::read_from_fd<opts.capture_stdout, opts.on_stdout_line_call_on_lines>(tok, fd, options.on_stdout_line);
                    if constexpr (opts.capture_stdout) {
                        std::scoped_lock lk(st->out_mtx);
                        st->stdout_ = std::move(txt);
                    }
                    internal::close_if_valid(fd);
                });
            }

            if constexpr (need_stderr) {
                st->stderr_thread = std::jthread([st, fd = err_pipe[0], &options](std::stop_token tok) mutable {
                    auto txt = internal::read_from_fd<opts.capture_stderr, opts.on_stderr_line_call_on_lines>(tok, fd, options.on_stderr_line);
                    if constexpr (opts.capture_stderr) {
                        std::scoped_lock lk(st->out_mtx);
                        st->stderr_ = std::move(txt);
                    }
                    internal::close_if_valid(fd);
                });
            }

            if (need_stdin && in_pipe[1] != -1) {
                st->stdin_thread = std::jthread([fd = in_pipe[1], &options](std::stop_token tok) mutable {
                    const char* p = options.input.data();
                    size_t rem = options.input.size();
                    while (rem > 0 && !tok.stop_requested()) {
                        size_t chunk = std::min<size_t>(rem, 64 * 1024);
                        ssize_t w = ::write(fd, p, chunk);
                        if (w < 0) { if (errno == EINTR) continue; break; }
                        if (w == 0) break;
                        p += (size_t)w; rem -= (size_t)w;
                    }
                    internal::close_if_valid(fd);
                });
            }

            st->waiter_thread = std::jthread([st, wait_ms = options.wait_for_ms, async_then = std::forward<decltype(options.async_then)>(options.async_then)](std::stop_token) {
                int status_raw = 0;
                bool waited_ok = false;

                if (wait_ms == 0xFFFFFFFFu) {
                    for (;;) {
                        pid_t r = ::waitpid(st->pid, &status_raw, 0);
                        if (r == st->pid) { waited_ok = true; break; }
                        if (r < 0 && errno == EINTR) continue;
                        break;
                    }
                } else {
                    auto start = std::chrono::steady_clock::now();
                    for (;;) {
                        pid_t r = ::waitpid(st->pid, &status_raw, WNOHANG);
                        if (r == st->pid) { waited_ok = true; break; }
                        if (r < 0 && errno == EINTR) continue;
                        if (r < 0) break;

                        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start
                        );
                        if ((uint32_t)elapsed.count() >= wait_ms) break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }

                    if (!waited_ok) {
                        if (st->pgid > 0) ::kill(-st->pgid, SIGTERM);
                        else ::kill(st->pid, SIGTERM);

                        std::this_thread::sleep_for(std::chrono::milliseconds(50));

                        pid_t r = ::waitpid(st->pid, &status_raw, WNOHANG);
                        if (r == st->pid) waited_ok = true;
                        else {
                            if (st->pgid > 0) ::kill(-st->pgid, SIGKILL);
                            else ::kill(st->pid, SIGKILL);

                            for (;;) {
                                pid_t rr = ::waitpid(st->pid, &status_raw, 0);
                                if (rr == st->pid) { waited_ok = true; break; }
                                if (rr < 0 && errno == EINTR) continue;
                                break;
                            }
                        }
                    }
                }

                int ec = -1;
                if (waited_ok) {
                    if (WIFEXITED(status_raw)) ec = WEXITSTATUS(status_raw);
                    else if (WIFSIGNALED(status_raw)) ec = 128 + WTERMSIG(status_raw);
                }

                st->status.store(ec, std::memory_order_release);
                st->finished.store(true, std::memory_order_release);
                if constexpr (has_async_next) {
                    std::invoke(async_then, ec);
                }
            });

            return out;
        }

        if constexpr (ONESHOT) {
            return one_shot_process_output{ 0, pid };
        }
        
        if constexpr (NORMAL) {
            std::string stdoutOutput;
            std::string stderrOutput;

            std::jthread stdoutThread;
            if constexpr (need_stdout) {
                stdoutThread = std::jthread([&](std::stop_token st) {
                    stdoutOutput = internal::read_from_fd<opts.capture_stdout, opts.on_stdout_line_call_on_lines>(
                        st, out_pipe[0], options.on_stdout_line
                    );
                    internal::close_if_valid(out_pipe[0]);
                    out_pipe[0] = -1;
                });
            }

            std::jthread stderrThread;
            if constexpr (need_stderr) {
                stderrThread = std::jthread([&](std::stop_token st) {
                    stderrOutput = internal::read_from_fd<opts.capture_stderr, opts.on_stderr_line_call_on_lines>(
                        st, err_pipe[0], options.on_stderr_line
                    );
                    internal::close_if_valid(err_pipe[0]);
                    err_pipe[0] = -1;
                });
            }

            std::jthread stdinThread;
            if (need_stdin && in_pipe[1] != -1) {
                stdinThread = std::jthread([&](std::stop_token st) {
                    const char* p = options.input.data();
                    size_t remaining = options.input.size();

                    while (remaining > 0 && !st.stop_requested()) {
                        size_t chunk = std::min<size_t>(remaining, 64 * 1024);
                        ssize_t w = ::write(in_pipe[1], p, chunk);
                        if (w < 0) {
                            if (errno == EINTR) continue;
                            break;
                        }
                        if (w == 0) break;
                        p += static_cast<size_t>(w);
                        remaining -= static_cast<size_t>(w);
                    }

                    internal::close_if_valid(in_pipe[1]);
                    in_pipe[1] = -1;
                });
            } else if (in_pipe[1] != -1) {
                internal::close_if_valid(in_pipe[1]);
                in_pipe[1] = -1;
            }

            int status_raw = 0;
            int exit_code = -1;
            bool waited_ok = false;

            if (options.wait_for_ms == 0xFFFFFFFFu) {
                for (;;) {
                    pid_t r = ::waitpid(pid, &status_raw, 0);
                    if (r == pid) { waited_ok = true; break; }
                    if (r < 0 && errno == EINTR) continue;
                    break;
                }
            } else {
                auto start = std::chrono::steady_clock::now();
                bool done = false;

                for (;;) {
                    pid_t r = ::waitpid(pid, &status_raw, WNOHANG);
                    if (r == pid) { waited_ok = true; done = true; break; }
                    if (r < 0) {
                        if (errno == EINTR) continue;
                        done = true;
                        break;
                    }

                    auto now = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
                    if (elapsed.count() >= options.wait_for_ms) break;

                    uint32_t remaining = options.wait_for_ms - static_cast<uint32_t>(elapsed.count());
                    uint32_t sleep_ms = std::min<uint32_t>(10, remaining);
                    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                }

                if (!waited_ok && !done) {
                    (void)::kill(pid, SIGTERM);

                    constexpr auto grace = std::chrono::milliseconds(50);
                    std::this_thread::sleep_for(grace);

                    pid_t r = ::waitpid(pid, &status_raw, WNOHANG);
                    if (r == pid) {
                        waited_ok = true;
                    } else {
                        (void)::kill(pid, SIGKILL);
                        for (;;) {
                            pid_t rr = ::waitpid(pid, &status_raw, 0);
                            if (rr == pid) { waited_ok = true; break; }
                            if (rr < 0 && errno == EINTR) continue;
                            break;
                        }
                    }
                }
            }

            if (waited_ok) {
                if (WIFEXITED(status_raw)) exit_code = WEXITSTATUS(status_raw);
                else if (WIFSIGNALED(status_raw)) exit_code = 128 + WTERMSIG(status_raw);
                else exit_code = -1;
            } else {
                exit_code = -1;
            }

            if (stdinThread.joinable())  stdinThread.request_stop();
            if (stdoutThread.joinable()) stdoutThread.request_stop();
            if (stderrThread.joinable()) stderrThread.request_stop();

            return process_output{ exit_code, std::move(stdoutOutput), std::move(stderrOutput) };
        }
    }
}

#endif

#pragma pop_macro("IS_NOOP")