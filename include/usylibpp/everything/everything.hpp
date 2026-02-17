#pragma once

#ifdef USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING

#include "../aliases.hpp" // IWYU pragma: export
#include "../util/worker.hpp"
#include "../windows/process.hpp"
#include "../windows.hpp"
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
        std::atomic_bool loaded{false};
        wil::unique_handle hjob;
        std::wstring instance_name;
    
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

        ~Everything() {
            Everything_Exit();
            Everything_CleanUp();
        }

        enum class LoadStatus {
            Success,
            NotRunning,

            NoExeFound,
            FailedToLaunchExe,
            OtherError
        };
        
        template <bool wait_for_completion = true>
        [[nodiscard]] inline auto try_load() {
            return worker.post<wait_for_completion>([this]() -> LoadStatus {
                if (!windows::exe_exists(L"everything.exe")) {
                    loaded.store(false);
                    return LoadStatus::NoExeFound;
                }

                auto status = windows::process::run_process<windows::process::process_options{
                    .allow_visible_windows = false,
                    .set_lifetime_of_subprocess_to_this_process = true,
                    .one_shot_process = true
                }>(windows::process::process_settings{
                    .commandline = strings::concat_strings( // instance name should always be valid?
                        L"everything.exe -instance \"", instance_name, L"\" "
                        L"-enable-run-as-admin -noapp-data -disable-update-notification -admin -startup"
                    )
                });

                if (status.status != 0) {
                    loaded.store(false);
                    return LoadStatus::OtherError;
                }

                hjob = std::move(status.hJob);

                std::this_thread::sleep_for(std::chrono::seconds(3));

                while (true) {
                    if (Everything_IsDBLoaded()) {
                        loaded.store(true);
                        return LoadStatus::Success;
                    } else if (Everything_GetLastError()) {
                        // IPC not running.
                        loaded.store(false);
                        return LoadStatus::NotRunning;
                    }
                    
                    // wait for database to load..
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                loaded.store(false);
                return LoadStatus::NotRunning;
            });
        }

        template <bool wait_for_completion = true>
        [[nodiscard]] inline auto is_loaded() {
            return worker.post<wait_for_completion>([]() -> bool {
                return Everything_IsDBLoaded();
            });
        }
    };
}
#endif