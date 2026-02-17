#pragma once

#ifdef USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING

#include "../aliases.hpp" // IWYU pragma: export
#include "../windows/process.hpp"
#include "../strings.hpp"

#include <Everything.h>
#include <everything_ipc.h>

namespace usylibpp {
    namespace EverythingExtra {
        /**
        * Set to the defaults
        */
        struct EverythingSearch {
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

        template <
            typename F1 = types::noop_t,
            typename F2 = types::noop_t,
            typename F3 = types::noop_t
        >
        requires (std::invocable<F1, DWORD> && 
                std::invocable<F2, DWORD> && 
                std::invocable<F3, DWORD>)
        struct Callbacks {
            F1 on_file{};
            F2 on_directory{};
            F3 on_volume{};
        };

        template <typename>
        struct is_callbacks : std::false_type {};

        template <typename F1, typename F2, typename F3>
        struct is_callbacks<Callbacks<F1, F2, F3>> : std::true_type {};

        template <typename T>
        concept CallbacksType = is_callbacks<std::remove_cvref_t<T>>::value;

        /**
         * Very basic query constructor
         * Can add a recursive directory and then exclude any amount of directories from it
         */
        struct Query {
            std::wstring q;

            constexpr explicit Query(std::wstring_view dir) {
                if (!q.empty()) q.push_back(L' ');
                q += L"path:\"";
                q += dir;
                if (!dir.ends_with(L'\\')) q.push_back(L'\\');
                q.push_back(L'\"');
            }

            constexpr inline Query& exclude_directory_absolute(std::wstring_view dir) {
                if (!q.empty()) q.push_back(L' ');
                q += L"!path:\"";
                q += dir;
                if (!dir.ends_with(L'*') && !dir.ends_with(L'\\')) q.push_back(L'\\');
                q.push_back(L'\"');

                return *this;
            }

            /**
             * Can't perform any other operations on this query
             */
            static constexpr inline Query from_directory_absolute(std::wstring_view dir) {
                Query query{dir};
                std::wstring excluded{dir};
                if (!excluded.ends_with(L'\\')) excluded.push_back(L'\\');
                excluded += L"*\\*";
                query.exclude_directory_absolute(excluded);

                return query;
            }

            const std::wstring get() const & noexcept {
                return q;
            }

            const std::wstring get() && noexcept {
                return std::move(q);
            }
        };
    }

    /**
     * Instance name is slightly sanatised. Don't trust it
     * Compile time error if known at compile time otherwise runtime error
     * Also only ever have one of these in an app
     */
    struct Everything {
    private:
        std::wstring instance_name;
        std::wstring everything_path = L"everything.exe";
        
        std::unique_ptr<std::wstring> wndclass;

        void reset_wndclass() {
            _Everything_IPC_WndClass = EVERYTHING_IPC_WNDCLASS;
            wndclass.reset();
        }

    static inline constexpr void validate_name(std::wstring_view name) {
        constexpr auto fail = [&](auto&& msg) {
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
            Everything_Exit();
            Everything_CleanUp();

            reset_wndclass();
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
        
        [[nodiscard]] inline LoadStatus try_load() {
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
        }

        [[nodiscard]] static inline auto is_loaded() {
            return Everything_IsDBLoaded();
        }

        inline void static reset_results() {
            Everything_Reset();
        }

        [[nodiscard]] static inline auto query_file_count() {
            return Everything_GetNumFileResults();
        }

        [[nodiscard]] static inline auto query_folder_count() {
            return Everything_GetNumFolderResults();
        }

        [[nodiscard]] static inline auto query_results_count() {
            return Everything_GetNumResults();
        }

        [[nodiscard]] static inline auto total_file_results() {
            return Everything_GetTotFileResults();
        }

        [[nodiscard]] static inline auto total_folder_results() {
            return Everything_GetTotFolderResults();
        }

        [[nodiscard]] static inline auto total_results() {
            return Everything_GetTotResults();
        }

        [[nodiscard]] static inline auto do_query(const std::wstring& query, const EverythingExtra::EverythingSearch& options = {}) {
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
        }

        /**
         * Use the Everything_Get... functions using the index to get the require data
         */
        template <EverythingExtra::CallbacksType CB>
        static inline void walk_results(CB&& cb) {
            const auto results_count = query_results_count();

            #pragma push_macro("HANDLE")
            #undef HANDLE
            #define HANDLE(checkresult, func) \
            if constexpr (!std::is_same_v<decltype(std::declval<CB>().func), types::noop_t>) { \
                if (checkresult(i)) { \
                    if constexpr (std::is_same_v<std::invoke_result_t<decltype(std::declval<CB>().func), DWORD>, bool>) { \
                        if (!std::invoke(cb.func, i)) break; \
                    } else { \
                        std::invoke(cb.func, i); \
                    } \
                    continue; \
                } \
            }

            for (DWORD i = 0; i < results_count; ++i) {
                HANDLE(Everything_IsFileResult, on_file);
                HANDLE(Everything_IsFolderResult, on_directory);
                HANDLE(Everything_IsVolumeResult, on_volume);
            }

            #pragma pop_macro("HANDLE")
        }
    };
}
#endif