#pragma once

#ifdef USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING
#ifndef USYLIBPP_ENABLE_WIL
#error "USYLIBPP_ENABLE_WIL must be enabled to use everything"
#endif

#include "../aliases.hpp" // IWYU pragma: export
#include "../windows/process.hpp"
#include "../windows/strings.hpp"
#include "../strings.hpp"
#include "../windows/char_t.hpp"

#include <Everything.h>
#include <everything_ipc.h>

#include <wil/resource.h>

namespace usylibpp {
    struct EverythingFile {
        const DWORD i;

        EverythingFile(DWORD _i) : i{_i} {}

        [[nodiscard]] inline auto filename() const noexcept {
            return Everything_GetResultFileName(i);
        }

        [[nodiscard]] inline auto filename_utf8() const {
            return windows::to_utf8(filename());
        }

        [[nodiscard]] inline auto filename_utf8_or_default() const {
            return filename_utf8().value_or(std::string{});
        }

        [[nodiscard]] inline auto parent_path() const noexcept {
            return Everything_GetResultPath(i);
        }

        [[nodiscard]] inline auto parent_path_utf8() const {
            return windows::to_utf8(parent_path());
        }

        [[nodiscard]] inline auto parent_path_utf8_or_default() const {
            return parent_path_utf8().value_or(std::string{});
        }

        [[nodiscard]] inline auto extension() const noexcept {
            return Everything_GetResultExtension(i);
        }

        [[nodiscard]] inline auto extension_utf8() const {
            return windows::to_utf8(extension());
        }

        [[nodiscard]] inline auto extension_utf8_or_default() const {
            return extension_utf8().value_or(std::string{});
        }

        [[nodiscard]] inline std::optional<uint64_t> size() const noexcept {
            LARGE_INTEGER s;
            if (!Everything_GetResultSize(i, &s)) return std::nullopt;
            return static_cast<uint64_t>(s.QuadPart); // need to check if valid
            // return (static_cast<uint64_t>(s.HighPart) << 32) | static_cast<uint32_t>(s.LowPart);
        }

        [[nodiscard]] inline std::optional<uint64_t> date_created() const noexcept {
            FILETIME s;
            if (!Everything_GetResultDateCreated(i, &s)) return std::nullopt;
            return wil::filetime::to_int64(s);
        }

        [[nodiscard]] inline std::optional<uint64_t> date_modified() const noexcept {
            FILETIME s;
            if (!Everything_GetResultDateModified(i, &s)) return std::nullopt;
            return wil::filetime::to_int64(s);
        }

        [[nodiscard]] inline std::optional<uint64_t> date_accessed() const noexcept {
            FILETIME s;
            if (!Everything_GetResultDateAccessed(i, &s)) return std::nullopt;
            return wil::filetime::to_int64(s);
        }

        [[nodiscard]] inline auto attributes() const noexcept {
            return Everything_GetResultAttributes(i);
        }

        [[nodiscard]] inline auto file_list_file_name() const noexcept {
            return Everything_GetResultFileListFileName(i);
        }

        [[nodiscard]] inline auto run_count() const noexcept {
            return Everything_GetResultRunCount(i);
        }

        [[nodiscard]] inline std::optional<uint64_t> date_run() const noexcept {
            FILETIME s;
            if (!Everything_GetResultDateRun(i, &s)) return std::nullopt;
            return wil::filetime::to_int64(s);
        }

        [[nodiscard]] inline std::optional<uint64_t> date_recently_changed() const noexcept {
            FILETIME s;
            if (!Everything_GetResultDateRecentlyChanged(i, &s)) return std::nullopt;
            return wil::filetime::to_int64(s);
        }

        [[nodiscard]] inline auto highlighted_file_name() const noexcept {
            return Everything_GetResultHighlightedFileName(i);
        }

        [[nodiscard]] inline auto highlighted_path() const noexcept {
            return Everything_GetResultHighlightedPath(i);
        }

        [[nodiscard]] inline auto highlighted_full_path_and_filename() const noexcept {
            return Everything_GetResultHighlightedFullPathAndFileName(i);
        }
    };

    namespace EverythingExtra {
        struct RequestFlags {
            DWORD flags{0};

            constexpr RequestFlags(DWORD flags = EVERYTHING_REQUEST_FILE_NAME | EVERYTHING_REQUEST_PATH) noexcept : flags{flags} {}

            constexpr void clear() noexcept {
                flags = 0;
            }

            #pragma push_macro("HANDLE")
            #undef HANDLE
            #define HANDLE(func_name, flag) \
            constexpr RequestFlags& func_name() noexcept { \
                flags |= flag; \
                return *this; \
            }

            HANDLE(file_name, EVERYTHING_REQUEST_FILE_NAME)
            HANDLE(path, EVERYTHING_REQUEST_PATH)
            HANDLE(full_path_and_file_name, EVERYTHING_REQUEST_FULL_PATH_AND_FILE_NAME)
            HANDLE(extension, EVERYTHING_REQUEST_EXTENSION)
            HANDLE(size, EVERYTHING_REQUEST_SIZE)
            HANDLE(date_created, EVERYTHING_REQUEST_DATE_CREATED)
            HANDLE(date_modified, EVERYTHING_REQUEST_DATE_MODIFIED)
            HANDLE(date_accessed, EVERYTHING_REQUEST_DATE_ACCESSED)
            HANDLE(attributes, EVERYTHING_REQUEST_ATTRIBUTES)
            HANDLE(file_list_file_name, EVERYTHING_REQUEST_FILE_LIST_FILE_NAME)
            HANDLE(run_count, EVERYTHING_REQUEST_RUN_COUNT)
            HANDLE(date_run, EVERYTHING_REQUEST_DATE_RUN)
            HANDLE(recently_changed, EVERYTHING_REQUEST_DATE_RECENTLY_CHANGED)
            HANDLE(highlighted_file_name, EVERYTHING_REQUEST_HIGHLIGHTED_FILE_NAME)
            HANDLE(highlighted_path, EVERYTHING_REQUEST_HIGHLIGHTED_PATH)
            HANDLE(highlighted_full_path_and_file_name, EVERYTHING_REQUEST_HIGHLIGHTED_FULL_PATH_AND_FILE_NAME)

            #pragma pop_macro("HANDLE")

            constexpr inline DWORD get() const noexcept {
                return flags;
            }
        };
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
            RequestFlags RequestFlags{};
        };

        template <
            typename F1 = types::noop_t,
            typename F2 = types::noop_t,
            typename F3 = types::noop_t
        >
        requires (std::invocable<F1, EverythingFile> && 
                std::invocable<F2, EverythingFile> && 
                std::invocable<F3, EverythingFile>)
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
            std::wstring base_dir;
            std::wstring q;

            constexpr explicit Query(std::wstring_view dir) : base_dir{dir} {
                if (!base_dir.ends_with(L'\\')) base_dir.push_back(L'\\');

                if (!q.empty()) q.push_back(L' ');
                q += L"path:\"";
                q += base_dir;
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

            constexpr inline Query& exclude_directory_any(std::wstring_view dir) {
                exclude_directory_absolute(strings::concat_strings(base_dir, dir));
                exclude_directory_absolute(strings::concat_strings(base_dir, L"*\\", dir));

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

            constexpr const std::wstring& get() const & noexcept {
                return q;
            }

            constexpr std::wstring&& get() && noexcept {
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
        #ifdef UNICODE
        std::wstring instance_name;
        std::wstring everything_path = L"everything.exe";
        std::unique_ptr<std::wstring> wndclass;
        #else
        std::string instance_name;
        std::string everything_path = "everything.exe";
        std::unique_ptr<std::string> wndclass;
        #endif

        void reset_wndclass() {
            _Everything_IPC_WndClass = EVERYTHING_IPC_WNDCLASS;
            wndclass.reset();
        }

    static inline constexpr void validate_name(std::basic_string_view<windows::WIN_CHAR> name) {
        constexpr auto fail = [&](auto&& msg) {
            throw std::invalid_argument(msg);
        };

        const size_t N = name.size();

        if (N == 0) 
            fail("Must provide an instance name!");

        if (N > 64)
            fail("Instance name too long!");

        if (name.front() == windows::WIN_CHAR(' ') || name.back() == windows::WIN_CHAR(' '))
            fail("Instance name cannot start or end with whitespace!");

        if (name.back() == windows::WIN_CHAR('.'))
            fail("Instance name cannot end with a dot!");

        for (wchar_t c : name) {
            if (c == windows::WIN_CHAR('"'))
                fail("Instance name cannot have quotes!");

            if (c < 0x20)
                fail("Instance name contains control characters!");

            #ifdef UNICODE
            if (std::wstring_view{L"/\\:*?|<> "} .find(c) != std::wstring_view::npos)
                fail("Instance name contains invalid path characters!");
            #else
            if (std::string_view{"/\\:*?|<> "} .find(c) != std::string_view::npos)
                fail("Instance name contains invalid path characters!");
            #endif
        }
    }

    struct instance_name {
    public:
        template <class T>
        requires std::convertible_to<const T&, std::basic_string_view<windows::WIN_CHAR>>
        consteval instance_name(const T& str) : _str(str) {
            validate_name(_str);
        }

        [[nodiscard]] constexpr std::basic_string_view<windows::WIN_CHAR> get() const noexcept {
            return _str;
        }

    private:
        std::basic_string_view<windows::WIN_CHAR> _str;
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

        Everything(struct instance_name _instance_name, std::basic_string<windows::WIN_CHAR> _everything_path) : instance_name{_instance_name.get()}, everything_path{std::move(_everything_path)} {
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

            #ifdef UNICODE
            const auto args = strings::concat_strings(L"-instance \"", instance_name, L"\" -admin -startup -is-run-as");
            #else
            const auto args = strings::concat_strings("-instance \"", instance_name, "\" -admin -startup -is-run-as");
            #endif
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

            #ifdef UNICODE
            wndclass = std::make_unique<std::wstring>(strings::concat_strings(EVERYTHING_IPC_WNDCLASS, L"_(", instance_name, L")"));
            #else
            wndclass = std::make_unique<std::string>(strings::concat_strings(EVERYTHING_IPC_WNDCLASS, "_(", instance_name, ")"));
            #endif

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

        [[nodiscard]] static inline auto is_loaded() noexcept {
            return Everything_IsDBLoaded();
        }

        inline void static reset_results() noexcept {
            Everything_Reset();
        }

        [[nodiscard]] static inline auto query_file_count() noexcept {
            return Everything_GetNumFileResults();
        }

        [[nodiscard]] static inline auto query_folder_count() noexcept {
            return Everything_GetNumFolderResults();
        }

        [[nodiscard]] static inline auto query_results_count() noexcept {
            return Everything_GetNumResults();
        }

        [[nodiscard]] static inline auto total_file_results() noexcept {
            return Everything_GetTotFileResults();
        }

        [[nodiscard]] static inline auto total_folder_results() noexcept {
            return Everything_GetTotFolderResults();
        }

        [[nodiscard]] static inline auto total_results() noexcept {
            return Everything_GetTotResults();
        }

        [[nodiscard]] static inline auto do_query(const std::basic_string<windows::WIN_CHAR>& query, const EverythingExtra::EverythingSearch& options = {}) noexcept {
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
            Everything_SetRequestFlags(options.RequestFlags.get());

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