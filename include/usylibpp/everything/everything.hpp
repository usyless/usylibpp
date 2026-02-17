#pragma once

#ifdef USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING

#include "../aliases.hpp" // IWYU pragma: export
#include "../util/worker.hpp"
#include "../windows/process.hpp"
#include "../strings.hpp"

#include <Everything.h>
#include <everything_ipc.h>

namespace usylibpp::everything {
    constexpr void validate_name(std::wstring_view name) {
        auto fail = [&](auto&& msg) {
            throw std::invalid_argument(msg);
        };

        const size_t N = name.size();

        if (N == 0) 
            fail("Must provide an instance name!");

        if (N > 64)
            fail("Instance name too long!");

        if (name.front() == L' ' || name.back() == L' ')
            fail("Instance name cannot start or end with whitespace!");

        if (name.back() == L'.')
            fail("Instance name cannot end with a dot!");

        for (wchar_t c : name) {
            if (c == L'"')
                fail("Instance name cannot have quotes!");

            if (c < 0x20)
                fail("Instance name contains control characters!");

            if (std::wstring_view{L"/\\:*?|<> "} .find(c) != std::wstring_view::npos)
                fail("Instance name contains invalid path characters!");
        }
    }

    struct instance_name {
    public:
        template <class T>
        requires std::convertible_to<const T&, std::wstring_view>
        consteval instance_name(const T& str) : _str(str) {
            validate_name(_str);
        }

        [[nodiscard]] constexpr std::wstring_view get() const noexcept {
            return _str;
        }

    private:
        std::wstring_view _str;
    };

    /**
     * Set to the defaults
     */
    struct SearchOptions {
        bool MatchPath{false};
        bool MatchCase{false};
        bool MatchWholeWord{false};
        bool Regex{false};
        DWORD Max{0xffffffff};
        DWORD Offset{0};
        // HWND ReplyWindow{nullptr};
        DWORD ReplyID{0};
        DWORD Sort{EVERYTHING_SORT_NAME_ASCENDING};
        DWORD RequestFlags{EVERYTHING_REQUEST_FILE_NAME | EVERYTHING_REQUEST_PATH};
    };

    /**
     * Instance name is slightly sanatised. Don't trust it
     * Compile time error if known at compile time otherwise runtime error
     * Also only ever have one of these in an app
     */
    struct Everything {
    private:
        util::Worker<util::WorkerOpts{
            .drain_queue_on_cancel = true,
            .type = util::WorkerType::ReturnDefault
        }> worker{1};
        std::wstring instance_name;
        std::wstring everything_path = L"everything.exe";
        
        std::unique_ptr<std::wstring> wndclass;

        void reset_wndclass() {
            _Everything_IPC_WndClass = EVERYTHING_IPC_WNDCLASS;
            wndclass.reset();
        }
    
    public:
        Everything(const Everything&) = delete;
        Everything& operator=(const Everything&) = delete;
        Everything(Everything&&) = delete;
        Everything& operator=(Everything&&) = delete;

        Everything(struct instance_name _instance_name) : instance_name{_instance_name.get()} {
            #if __cplusplus >= 202302L
            if consteval {} else
            #else
            if (!std::is_constant_evaluated())
            #endif
            {
                validate_name(instance_name);
            }
        }

        Everything(struct instance_name _instance_name, std::wstring _everything_path) : instance_name{_instance_name.get()}, everything_path{std::move(_everything_path)} {
            #if __cplusplus >= 202302L
            if consteval {} else
            #else
            if (!std::is_constant_evaluated())
            #endif
            {
                validate_name(instance_name);
            }
        }

        template <bool wait_for_completion = true>
        [[nodiscard]] inline auto close_everything() {
            return worker.post<wait_for_completion>([this]{
                Everything_Exit();
                Everything_CleanUp();

                reset_wndclass();
            });
        }

        ~Everything() {
            close_everything();
        }

        enum class LoadStatus {
            Success,
            NotRunning,

            NoExeFound,
            FailedToLaunchExe,
            OtherError,

            UACRejected
        };
        
        template <bool wait_for_completion = true>
        [[nodiscard]] inline auto try_load() {
            return worker.post<wait_for_completion>([this]() -> LoadStatus {
                if (everything_path.empty()) {
                    return LoadStatus::NoExeFound;
                }

                const auto args = strings::concat_strings(L"-instance \"", instance_name, L"\" -admin -startup -is-run-as");
                auto status = windows::process::run_admin_process<{
                    .allow_visible_windows = true,
                }>({
                    .filename = &everything_path,
                    .args = &args
                });

                if (status.status != windows::process::admin_process_output::Status::Success) {
                    if (status.status == windows::process::admin_process_output::Status::UACRejected) {
                        return LoadStatus::UACRejected;
                    }
                    return LoadStatus::OtherError;
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));

                wndclass = std::make_unique<std::wstring>(strings::concat_strings(EVERYTHING_IPC_WNDCLASS, L"_(", instance_name, L")"));

                _Everything_IPC_WndClass = wndclass->c_str();

                while (true) {
                    if (Everything_IsDBLoaded()) {
                        return LoadStatus::Success;
                    } else if (Everything_GetLastError()) {
                        // IPC not running.
                        reset_wndclass();
                        return LoadStatus::NotRunning;
                    }
                    
                    // wait for database to load..
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                reset_wndclass();
                return LoadStatus::NotRunning;
            });
        }

        template <bool wait_for_completion = true>
        [[nodiscard]] inline auto is_loaded() {
            return worker.post<wait_for_completion>([]() -> bool {
                return Everything_IsDBLoaded();
            });
        }

        template <bool wait_for_completion = true>
        [[nodiscard]] inline auto reset_results() {
            return worker.post<wait_for_completion>([]{
                Everything_Reset();
            });
        }

        template <bool wait_for_completion = true>
        [[nodiscard]] inline auto do_query(const std::wstring& query, const SearchOptions& options = {}) {
            constexpr auto do_query = [](const std::wstring& query, const SearchOptions& options) {
                Everything_Reset();

                Everything_SetMatchPath(options.MatchPath);
                Everything_SetMatchCase(options.MatchCase);
                Everything_SetMatchWholeWord(options.MatchWholeWord);
                Everything_SetRegex(options.Regex);
                Everything_SetMax(options.Max);
                Everything_SetOffset(options.Offset);
                // Everything_SetReplyWindow(options.ReplyWindow);
                Everything_SetReplyID(options.ReplyID);
                Everything_SetSort(options.Sort);
                Everything_SetRequestFlags(options.RequestFlags);

                Everything_SetSearch(query.c_str());

                return Everything_Query(TRUE);
            };
            if constexpr (wait_for_completion) {
                return worker.post<wait_for_completion>([&query, &options]{
                    return do_query(query, options);
                });
            } else {
                return worker.post<wait_for_completion>([query, options]{
                    return do_query(query, options);
                });
            }
        }
    };
}
#endif