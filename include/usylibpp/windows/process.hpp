#pragma once

#ifdef USYLIBPP_ENABLE_WIL
#include <string>
#include <thread>
#include <functional>
#include <filesystem>
#include <windows.h>

#include <wil/resource.h>
#include <wil/com.h>

namespace usylibpp::windows::process {
    namespace internal {
        /**
            * The callback is a function which takes one argument of std::string_view
            */
        template <bool with_output = true, bool break_line = true, typename Callback = std::nullptr_t>
        inline std::string read_from_pipe(std::stop_token stop, HANDLE pipe, Callback&& on_line = nullptr) {
            std::string output;
            char buffer[4096];
            DWORD bytesRead{0};

            std::string partialLine;

            std::stop_callback cb(stop, [&pipe, thread = GetCurrentThread()]() {
                CancelSynchronousIo(thread);
            });

            while (ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                if (stop.stop_requested()) break;

                if constexpr (with_output) output.append(buffer, bytesRead);

                if constexpr (!std::is_same_v<Callback, std::nullptr_t>) {
                    if constexpr (!break_line) {
                        on_line(std::string_view{buffer, bytesRead});
                    } else {
                        partialLine.append(buffer, bytesRead);

                        std::size_t start_pos = 0;
                        std::size_t new_line_pos = 0;
                        while ((new_line_pos = partialLine.find('\n', new_line_pos)) != std::string::npos) {
                            std::size_t line_end = new_line_pos;

                            if (line_end > start_pos && partialLine[line_end - 1] == '\r') --line_end;

                            std::string_view line{partialLine.data() + start_pos, line_end - start_pos};

                            on_line(line);
                            start_pos = ++new_line_pos;
                        }

                        partialLine.erase(0, start_pos);
                    }
                }
            }

            if constexpr (!std::is_same_v<Callback, std::nullptr_t> && break_line) {
                if (!partialLine.empty()) {
                    on_line(partialLine);
                }
            }

            return output;
        }
    }

    struct process_output {
        int status = 0;
        std::string stdout_{};
        std::string stderr_{};
    };

    struct process_settings {
        std::wstring_view commandline;
        std::string_view input = "";
        std::filesystem::path* working_directory = nullptr;

        std::function<void(std::string_view)> on_stdout_line = nullptr;
        std::function<void(std::string_view)> on_stderr_line = nullptr;

        DWORD wait_for_ms = INFINITE;
    };

    struct process_options {
        bool allow_visible_windows = true;
        bool capture_stdout = true;
        bool capture_stderr = true;
        bool set_lifetime_of_subprocess_to_this_process = true;
        bool on_stdout_line_call_on_lines = true; // otherwise only call on buffer full
        bool on_stderr_line_call_on_lines = true;
    };

    /**
        * Run a process and either capture its output or dont
        * Blocks until the process exits
        */
    template <process_options opts = {}>
    inline process_output run_process(const process_settings& options) {
        if (options.commandline.empty()) {
            return { -1 };
        }
        
        SECURITY_ATTRIBUTES saAttr{};
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        wil::unique_handle hStdOutRead = NULL, hStdOutWrite = NULL;
        wil::unique_handle hStdErrRead = NULL, hStdErrWrite = NULL;
        wil::unique_handle hStdInRead = NULL, hStdInWrite = NULL;

        {
        HANDLE hReadOut = NULL, hWriteOut = NULL;
        HANDLE hReadErr = NULL, hWriteErr = NULL;
        HANDLE hInRead = NULL, hInWrite = NULL;
        
        if (opts.capture_stdout || options.on_stdout_line) {
            if (!CreatePipe(&hReadOut, &hWriteOut, &saAttr, 0)) {
                return { -1 };
            }
        }

        hStdOutRead.reset(hReadOut);
        hStdOutWrite.reset(hWriteOut);

        if (opts.capture_stdout || options.on_stdout_line) {
            if (!SetHandleInformation(hStdOutRead.get(), HANDLE_FLAG_INHERIT, 0)) {
                return { -1 };
            }
        }

        if (opts.capture_stderr || options.on_stderr_line) {
            if (!CreatePipe(&hReadErr, &hWriteErr, &saAttr, 0)) {
                return { -1 };
            }
        }

        hStdErrRead.reset(hReadErr);
        hStdErrWrite.reset(hWriteErr);

        if (opts.capture_stderr || options.on_stderr_line) {
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

        if (!options.input.empty() and hStdInWrite.is_valid()) {
            DWORD written;
            WriteFile(hStdInWrite.get(), options.input.data(), static_cast<DWORD>(options.input.size()), &written, NULL);
            hStdInWrite.reset();
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

        std::string stdoutOutput;
        std::string stderrOutput;

        {
        std::jthread stdoutThread;
        if (opts.capture_stdout || options.on_stdout_line) {
            stdoutThread = std::jthread([&stdoutOutput, hStdOutRead = hStdOutRead.get(), &options](std::stop_token st) {
                if (options.on_stdout_line) stdoutOutput = internal::read_from_pipe<opts.capture_stdout, opts.on_stdout_line_call_on_lines>(st, hStdOutRead, options.on_stdout_line);
                else stdoutOutput = internal::read_from_pipe<opts.capture_stdout>(st, hStdOutRead);
            });
        }

        std::jthread stderrThread;
        if (opts.capture_stderr || options.on_stderr_line) {
            stderrThread = std::jthread([&stderrOutput, hStdErrRead = hStdErrRead.get(), &options](std::stop_token st) {
                if (options.on_stderr_line) stderrOutput = internal::read_from_pipe<opts.capture_stderr, opts.on_stderr_line_call_on_lines>(st, hStdErrRead, options.on_stderr_line);
                else stderrOutput = internal::read_from_pipe<opts.capture_stderr>(st, hStdErrRead);
            });
        }

        DWORD result = WaitForSingleObject(process.get(), options.wait_for_ms);

        if (result == WAIT_TIMEOUT) { 
            TerminateProcess(process.get(), 1); 
            WaitForSingleObject(process.get(), INFINITE);

            if (stdoutThread.joinable()) stdoutThread.request_stop();
            if (stderrThread.joinable()) stderrThread.request_stop();
        }
        }

        DWORD exitCode = 0;
        GetExitCodeProcess(process.get(), &exitCode);

        return { static_cast<int>(exitCode), stdoutOutput, stderrOutput };
    }
}
#endif