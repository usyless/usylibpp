#pragma once

#include "../types.hpp"

namespace usylibpp::windows::process {
    struct process_output {
        int status = -1;
        std::string stdout_{};
        std::string stderr_{};
    };

    struct process_options {
        bool allow_visible_windows = true;
        bool capture_stdout = true;
        bool capture_stderr = true;
        bool set_lifetime_of_subprocess_to_this_process = true;
        bool on_stdout_line_call_on_lines = true; // otherwise only call on buffer full
        bool on_stderr_line_call_on_lines = true;

        bool one_shot_process = false; // no stdout or stderr
    };
}

// only true if windows
// linux at the end of the file
#ifdef USYLIBPP_ENABLE_WIL
#include "../aliases.hpp" // IWYU pragma: export
#include <string>
#include <thread>
#include <functional>
#include <filesystem>
#include <windows.h>
#include <shellapi.h>
#include "char_t.hpp"

#include <wil/resource.h>
#include <wil/com.h>

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

    struct one_shot_process_output {
        int status = -1; // 0 if no early error
        wil::unique_handle hJob;
    };

    template <
        typename F1 = types::noop_t,
        typename F2 = types::noop_t
    >
    requires (std::invocable<F1&, std::string_view> && std::invocable<F2&, std::string_view>)
    struct process_settings {
        std::wstring_view commandline;
        std::string_view input = "";
        std::filesystem::path* working_directory = nullptr;

        F1 on_stdout_line{};
        F2 on_stderr_line{};

        DWORD wait_for_ms = INFINITE;
    };

    template <typename>
    struct is_process_settings : std::false_type {};

    template <typename F1, typename F2>
    struct is_process_settings<process_settings<F1, F2>> : std::true_type {};

    template <typename T>
    concept process_settings_type = is_process_settings<std::remove_cvref_t<T>>::value;

    /**
        * Run a process and either capture its output or dont
        * Blocks until the process exits
        */
    template <process_options opts = {}, process_settings_type settings>
    inline std::conditional_t<opts.one_shot_process, one_shot_process_output, process_output> run_process(settings&& options) {
        if (options.commandline.empty()) {
            return { -1 };
        }

        #pragma push_macro("IS_NOOP")
        #undef IS_NOOP
        #define IS_NOOP(func) (std::is_same_v<decltype(std::declval<settings>().func), types::noop_t>)
        
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
                    return { -1 };
                }
            }

            hStdOutRead.reset(hReadOut);
            hStdOutWrite.reset(hWriteOut);

            if constexpr (opts.capture_stdout || !IS_NOOP(on_stdout_line)) {
                if (!SetHandleInformation(hStdOutRead.get(), HANDLE_FLAG_INHERIT, 0)) {
                    return { -1 };
                }
            }

            if constexpr (opts.capture_stderr || !IS_NOOP(on_stderr_line)) {
                if (!CreatePipe(&hReadErr, &hWriteErr, &saAttr, 0)) {
                    return { -1 };
                }
            }

            hStdErrRead.reset(hReadErr);
            hStdErrWrite.reset(hWriteErr);

            if constexpr (opts.capture_stderr || !IS_NOOP(on_stderr_line)) {
                if (!SetHandleInformation(hStdErrRead.get(), HANDLE_FLAG_INHERIT, 0)) {
                    return { -1 };
                }
            }

            if (!options.input.empty()) {
                if (!CreatePipe(&hInRead, &hInWrite, &saAttr, 0)) {
                    return { -1 };
                }
                
                hStdInRead.reset(hInRead);
                hStdInWrite.reset(hInWrite);

                if (!SetHandleInformation(hStdInWrite.get(), HANDLE_FLAG_INHERIT, 0)) {
                    return { -1 };
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
            return { -1 };
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

        if constexpr (!opts.one_shot_process) {
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

            #pragma pop_macro("IS_NOOP")

            return process_output{ static_cast<int>(exitCode), stdoutOutput, stderrOutput };
        } else {
            return one_shot_process_output{ 0, std::move(hJob) };
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
#include <atomic>
#include <cerrno>
#include <csignal>
#include <concepts>
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

        inline int make_pipe_cloexec(int fds[2]) {
#if defined(__linux__)
            if (::pipe2(fds, O_CLOEXEC) == 0) return 0;
            if (errno != ENOSYS) return -1;
#endif
            if (::pipe(fds) != 0) return -1;
            ::fcntl(fds[0], F_SETFD, FD_CLOEXEC);
            ::fcntl(fds[1], F_SETFD, FD_CLOEXEC);
            return 0;
        }
    }

    struct one_shot_process_output {
        int status = -1;      // 0 on successful spawn
        pid_t pid = -1;
    };

    template <typename F1 = types::noop_t, typename F2 = types::noop_t>
    requires (std::invocable<F1&, std::string_view> && std::invocable<F2&, std::string_view>)
    struct process_settings {
        std::string_view commandline;
        std::string_view input = "";
        std::filesystem::path* working_directory = nullptr;

        F1 on_stdout_line{};
        F2 on_stderr_line{};

        uint32_t wait_for_ms = 0xFFFFFFFFu; // INFINITE-like
    };

    template <typename>
    struct is_process_settings : std::false_type {};

    template <typename F1, typename F2>
    struct is_process_settings<process_settings<F1, F2>> : std::true_type {};

    template <typename T>
    concept process_settings_type = is_process_settings<std::remove_cvref_t<T>>::value;

    template <process_options opts = {}, process_settings_type settings>
    inline std::conditional_t<opts.one_shot_process, one_shot_process_output, process_output>
    run_process(settings&& options) {
        if (options.commandline.empty()) return { -1 };

        constexpr bool need_stdout = opts.capture_stdout || !std::is_same_v<decltype(options.on_stdout_line), types::noop_t>;
        constexpr bool need_stderr = opts.capture_stderr || !std::is_same_v<decltype(options.on_stderr_line), types::noop_t>;
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
                    if (out_pipe[0] != -1) { ::close(out_pipe[0]); ::close(out_pipe[1]); }
                    return { -1 };
                }
            }
            if (need_stdin) {
                if (internal::make_pipe_cloexec(in_pipe) != 0) {
                    if (out_pipe[0] != -1) { ::close(out_pipe[0]); ::close(out_pipe[1]); }
                    if (err_pipe[0] != -1) { ::close(err_pipe[0]); ::close(err_pipe[1]); }
                    return { -1 };
                }
            }
        }

        pid_t pid = ::fork();
        if (pid < 0) {
            if (out_pipe[0] != -1) { ::close(out_pipe[0]); ::close(out_pipe[1]); }
            if (err_pipe[0] != -1) { ::close(err_pipe[0]); ::close(err_pipe[1]); }
            if (in_pipe[0] != -1) { ::close(in_pipe[0]); ::close(in_pipe[1]); }
            return { -1 };
        }

        if (pid == 0) {
            // Child
            if constexpr (!opts.one_shot_process) {
                if constexpr (need_stdout) {
                    ::dup2(out_pipe[1], STDOUT_FILENO);
                }
                if constexpr (need_stderr) {
                    ::dup2(err_pipe[1], STDERR_FILENO);
                }
                if (need_stdin) {
                    ::dup2(in_pipe[0], STDIN_FILENO);
                }
            }

            if (out_pipe[0] != -1) { ::close(out_pipe[0]); ::close(out_pipe[1]); }
            if (err_pipe[0] != -1) { ::close(err_pipe[0]); ::close(err_pipe[1]); }
            if (in_pipe[0] != -1) { ::close(in_pipe[0]); ::close(in_pipe[1]); }

            if (options.working_directory && !options.working_directory->empty()) {
                (void)::chdir(options.working_directory->c_str());
            }

#if defined(__linux__)
            if constexpr (opts.set_lifetime_of_subprocess_to_this_process) {
                // Best-effort: kill child if parent dies (Linux-specific)
                (void)::prctl(PR_SET_PDEATHSIG, SIGKILL);
            }
#endif

            std::string cmd{options.commandline};
            ::execl("/bin/sh", "sh", "-c", cmd.c_str(), (char*)nullptr);
            _exit(127);
        }

        // Parent
        if constexpr (!opts.one_shot_process) {
            if (out_pipe[1] != -1) ::close(out_pipe[1]);
            if (err_pipe[1] != -1) ::close(err_pipe[1]);
            if (in_pipe[0] != -1) ::close(in_pipe[0]);
        }

        if constexpr (opts.one_shot_process) {
            return one_shot_process_output{ 0, pid };
        } else {
            std::string stdoutOutput;
            std::string stderrOutput;

            // Start draining stdout/stderr first to avoid pipe deadlocks.
            std::jthread stdoutThread;
            if constexpr (need_stdout) {
                stdoutThread = std::jthread([&](std::stop_token st) {
                    stdoutOutput = internal::read_from_fd<opts.capture_stdout, opts.on_stdout_line_call_on_lines>(
                        st, out_pipe[0], options.on_stdout_line
                    );
                    ::close(out_pipe[0]);
                });
            }

            std::jthread stderrThread;
            if constexpr (need_stderr) {
                stderrThread = std::jthread([&](std::stop_token st) {
                    stderrOutput = internal::read_from_fd<opts.capture_stderr, opts.on_stderr_line_call_on_lines>(
                        st, err_pipe[0], options.on_stderr_line
                    );
                    ::close(err_pipe[0]);
                });
            }

            // Write stdin concurrently (instead of blocking main thread before readers start).
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

                    ::close(in_pipe[1]);
                });
            } else if (in_pipe[1] != -1) {
                ::close(in_pipe[1]);
            }

            int status_raw = 0;
            int exit_code = -1;

            if (options.wait_for_ms == 0xFFFFFFFFu) {
                if (::waitpid(pid, &status_raw, 0) < 0) exit_code = -1;
                else if (WIFEXITED(status_raw)) exit_code = WEXITSTATUS(status_raw);
                else if (WIFSIGNALED(status_raw)) exit_code = 128 + WTERMSIG(status_raw);
            } else {
                const uint32_t step_ms = 10;
                uint32_t waited = 0;
                bool done = false;

                while (waited < options.wait_for_ms) {
                    pid_t r = ::waitpid(pid, &status_raw, WNOHANG);
                    if (r == pid) { done = true; break; }
                    if (r < 0 && errno != EINTR) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
                    waited += step_ms;
                }

                if (!done) {
                    ::kill(pid, SIGTERM);
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    if (::waitpid(pid, &status_raw, WNOHANG) == 0) {
                        ::kill(pid, SIGKILL);
                    }
                    ::waitpid(pid, &status_raw, 0);
                }

                if (WIFEXITED(status_raw)) exit_code = WEXITSTATUS(status_raw);
                else if (WIFSIGNALED(status_raw)) exit_code = 128 + WTERMSIG(status_raw);
            }

            if (stdinThread.joinable())  stdinThread.request_stop();
            if (stdoutThread.joinable()) stdoutThread.request_stop();
            if (stderrThread.joinable()) stderrThread.request_stop();

            return process_output{ exit_code, std::move(stdoutOutput), std::move(stderrOutput) };
        }
    }
}

#endif