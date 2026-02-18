#include <usylibpp/usylibpp.hpp>

int main() {
    using namespace usylibpp;

    print::println("Hello! Welcome to my silly library\n");

    print::println("Time functions:");
    print::println("time::datetime_string: {}", time::datetime_string());
    print::println();

    print::println("String functions:");
    print::println("strings::concat_strings (chars): {}", strings::concat_strings("hello", " ", "there!"));
    {
        constexpr auto result = strings::concat_strings("hello", " ", "there!");
        print::println("constexpr strings::concat_strings (chars): {}", result);
    }
    print::println("strings::constexpr_strlen (\"hi!\"): {}", strings::constexpr_strlen("hi!"));
    // print::println(L"strings::concat_strings (wide chars): {}", strings::concat_strings(L"hello", L" ", L"there!"));
    {
        auto str = "THis IS moSTLY upperCASE";
        print::println("strings::to_lowercase before: {}, after: {}", str, strings::to_lowercase(str));
    }
    {
        auto str = "this has THIS STrING and once again THIS STrING";
        print::println("strings::replace_all before: {}, after: {}", str, strings::replace_all(str, "THIS STrING", "not that string"));
    }
    print::println("strings::to_number_or_default<size_t> {}", strings::to_number_or_default<size_t>("1234567"));
    print::println("strings::to_number_or_default<double> {}", strings::to_number_or_default<double>("-123.456"));
    print::println("strings::to_string_view_or_default {}", strings::to_string_view_or_default(12234ULL));
    {
        auto str = "?this_is_a_get=lol a space??&ts=!!!%";
        print::println("strings::url_encode before: {}, after: {}", str, strings::url_encode(str));
    }
    {
        auto str = "https://example.com/what lol/true?this_is_a_get=lol a=space??&ts=!!!%#fragment";
        print::println("strings::encode_full_url before: {}, after: {}", str, strings::encode_full_url(str));
    }
    {
        auto str = "https://example.com/what lol/true?this_is_a_get=lol a=space??&ts=!!!%#fragment";
        print::println("strings::encode_full_url before: {}, after: {}", str, strings::encode_full_url(str));
    }
    {
        auto str = "https://example.com/what lol/true/#fragment";
        print::println("strings::encode_full_url before: {}, after: {}", str, strings::encode_full_url(str));
    }
    {
        auto str = "https://example.com?hi#fragment";
        print::println("strings::encode_full_url before: {}, after: {}", str, strings::encode_full_url(str));
    }
    print::println();

    #ifdef WIN32
    print::println("Windows functions:");
    // These break the vscode terminal
    // {
    //     auto str = "a not wide string: 你好";
    //     print::println("windows::to_wstr input: {}", str);
    //     print::println(L"windows::to_wstr output: {}", *windows::to_wstr(str));
    // }
    // {
    //     auto str = L"a not wide string: 你好";
    //     print::println(L"windows::to_utf8 input: {}", str);
    //     print::println("windows::to_utf8 output: {}", *windows::to_utf8(str));
    // }
    print::println("windows::to_utf8<{{.as_optional = false}}>: {}", windows::to_utf8<{.as_optional = false}>(std::wstring(L"This is a test wide string")));
    print::println("windows::current_executable_path_or_default: {}", windows::to_utf8_or_default(windows::current_executable_path_or_default().get()));
    print::println("windows::set_cwd_to_executable_directory: {}", windows::set_cwd_to_executable_directory());
    print::println("windows::get_known_folder_or_default: {}", windows::to_utf8_or_default(windows::get_known_folder_or_default()));
    print::println("windows::exe_exists(L\"usylibpp_test\"): {}", windows::exe_exists(L"usylibpp_test"));
    print::println("windows::exe_exists(L\"usylibpp\"): {}", windows::exe_exists(L"usylibpp"));
    print::println("windows::exe_exists(L\"ffmpeg\"): {}", windows::exe_exists(L"ffmpeg"));
    print::println("windows::admin::is_admin: {}", windows::admin::is_admin());

    {
        print::println("\nProcess (whoami):");
        const auto [status, output, error] = windows::process::run_process<windows::process::process_options{
            .allow_visible_windows = false,
            .capture_stdout = true,
            .capture_stderr = true,
            .set_lifetime_of_subprocess_to_this_process = true,
        }>(windows::process::process_settings{
            .commandline = L"whoami",
            .on_stdout_line = [](std::string_view line) {
                print::println("windows::process::run_process(whoami) - stdout line recieved: {}", line);
            }
        });
        print::println("windows::process::run_process(whoami) - status: {} ; output: {} ; error: {}", status, output, error);
    }
    #endif

    #ifdef USYLIBPP_ENABLE_WINTOAST
    using namespace WinToastLib;
    print::println("WinToast stuff:");

    wintoast::ToastWorker<true, util::WorkerType::ReturnDefault> toast{L"usylibpp_test", L"usy", L"usylibpp", L"usylibpp_test", L"20260206"};
    if (!toast.success()) {
        print::println("Failed to setup wintoast!");
    } else {
        WinToastTemplate templ(WinToastTemplate::Text02);
        templ.setTextField(L"Hello!", WinToastTemplate::FirstLine);
        templ.setTextField(L"This is a test toast.", WinToastTemplate::SecondLine);
        templ.setDuration(WinToastTemplate::Duration::Short);

        // new usage is fine here as it takes shared ownership after
        const auto id = toast.showToast(templ, new wintoast::PrintingWinToastHandler);

        const auto& appname = toast.appName();
        auto appnamefuture = toast.appName<false>();
        print::println("App name: {}", windows::to_utf8_or_default(appname));
        print::println("App name future: {}", windows::to_utf8_or_default(appnamefuture.get()));
        
        if (id < 0) {
            print::println("Toast failed");
        } else {
            print::println("Toast shown, id = {} (sleeping for 5 seconds)", id);
            Sleep(5000);
        }
    }
    #endif
    #ifdef USYLIBPP_ENABLE_WINDOWS_IMAGING
    print::println("Windows imaging stuff:");
    {
        auto supported_file_extensions = windows::images::get_all_supported_file_extensions();
        print::print("WIC supported file extensions: ");
        for (const auto& format : supported_file_extensions) {
            print::print("{}, ", windows::to_utf8_or_default(format));
        }
        print::println("");
    }
    {
        auto wic_test_png_result = windows::images::decode_image<false, windows::images::DecodeOpts{
            .type = windows::images::DecodedImageType::Gray
        }>(L"test.png");
        if (!wic_test_png_result) print::println("Failed to decode test.png! Ensure it exists");
        else {
            auto& res = wic_test_png_result.value();
            print::println("Decoded test.png, data size: {}, channels: {}, width: {}, height: {}", res.data.size(), res.channels, res.dimensions.width, res.dimensions.height);
        }
    }
    {
        windows::COMWrapper<false> COM{COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE};
        auto wic_test_png_result = windows::images::decode_image_as_view<windows::images::DecodeOpts{
            .type = windows::images::DecodedImageType::Gray
        }>(L"test.png");
        if (!wic_test_png_result) print::println("Failed to decode test.png! Ensure it exists");
        else {
            auto& res = wic_test_png_result.value();
            print::println("Decoded test.png, data size: {}, channels: {}, width: {}, height: {}", res.buffer_size, res.channels, res.dimensions.width, res.dimensions.height);
        }
    }
    #endif
    #ifdef USYLIBPP_ENABLE_WIL
    print::println("Windows fs stuff:");
    {
        print::println("All files in current folder:");
        windows::fs::walk_directory<windows::fs::WalkOpts{.recursive = true}>(L"..", windows::fs::Callbacks{
            .on_file = [](const std::wstring& parent, const windows::fs::FindDataWrapper& data) {
                print::println("File - Parent: {} ; Filename: {} ; Last Write time: {}", windows::to_utf8_or_default(parent), data.filename_utf8_or_default(), data.date_modified());
            },
            .on_directory = [](const std::wstring& parent, const windows::fs::FindDataWrapper& data) {
                print::println("Directory - Parent: {} ; Filename: {} ; Last Write time: {}", windows::to_utf8_or_default(parent), data.filename_utf8_or_default(), data.date_modified());

                auto filename = data.filename_view();
                if (filename == L".cmake" || filename == L"_deps" || filename == L".cache" || filename == L"CMakeFiles") return false;

                return true;
            },
            .on_other = [](const std::wstring& parent, const windows::fs::FindDataWrapper& data) {
                print::println("Other - Parent: {} ; Filename: {}", windows::to_utf8_or_default(parent), data.filename_utf8_or_default());
            },
        });
    }
    #endif

    #ifdef USYLIBPP_ENABLE_VOIDTOOLS_EVERYTHING
    print::println("Everything stuff (make sure to put everything.exe in everything\\everything.exe in the directory of the executable):");
    {
        print::println("Everything is loading... this may take a while for the first run");
        Everything everything{L"usylibpp_test", L"everything\\everything.exe"};
        switch (everything.try_load()) {
            case Everything::LoadStatus::Success: {
                print::println("Successfully loaded everything!");
                const auto executable_path_opt = windows::current_executable_path();
                if (executable_path_opt) {
                    const auto build_folder = executable_path_opt->get().parent_path().parent_path();
                    const auto query1 = EverythingExtra::Query::from_directory_absolute(build_folder.native());

                    print::println("Performing query: {}", windows::to_utf8_or_default(query1.get()));
                    if (everything.do_query(query1.get())) {
                        print::println("Query success!");

                        print::println("File count: {} ; Directory count: {} ; Total results count: {}", everything.query_file_count(), everything.query_folder_count(), everything.query_results_count());
                        everything.walk_results(EverythingExtra::Callbacks{
                            .on_file = [](const EverythingFile i) {
                                print::println("File result filename: {}", windows::to_utf8_or_default(i.filename()));
                            },
                            .on_directory = [](const EverythingFile i) {
                                print::println("Directory result filename: {}", windows::to_utf8_or_default(i.filename()));
                            },
                            .on_volume = [](const EverythingFile i) {
                                print::println("Volume result filename: {}", windows::to_utf8_or_default(i.filename()));
                            }
                        });
                    } else {
                        print::println("Query failed!");
                    }

                    print::println();

                    const auto query2 = 
                        EverythingExtra::Query(build_folder.native())
                        .exclude_directory_any(L".cmake")
                        .exclude_directory_any(L"_deps")
                        .exclude_directory_any(L".cache")
                        .exclude_directory_any(L"CMakeFiles");

                    print::println("Performing query: {}", windows::to_utf8_or_default(query2.get()));
                    if (everything.do_query(query2.get())) {
                        print::println("Query success!");

                        print::println("File count: {} ; Directory count: {} ; Total results count: {}", everything.query_file_count(), everything.query_folder_count(), everything.query_results_count());
                        everything.walk_results(EverythingExtra::Callbacks{
                            .on_file = [](const EverythingFile i) {
                                print::println("File result filename: {}", windows::to_utf8_or_default(i.filename()));
                            },
                            .on_directory = [](const EverythingFile i) {
                                print::println("Directory result filename: {}", windows::to_utf8_or_default(i.filename()));
                            },
                            .on_volume = [](const EverythingFile i) {
                                print::println("Volume result filename: {}", windows::to_utf8_or_default(i.filename()));
                            }
                        });
                    } else {
                        print::println("Query failed!");
                    }
                } else {
                    print::println("Failed to get executable path! Not testing everything query...");
                }
                break;
            }
            case Everything::LoadStatus::FailedToLaunchExe: {
                print::println("Failed to launch everything exe!");
                break;
            }
            case Everything::LoadStatus::NoExeFound: {
                print::println("Everything exe not found!");
                break;
            }
            case Everything::LoadStatus::NotRunning: {
                print::println("Everything failed to run (not running)!");
                break;
            }
            case Everything::LoadStatus::OtherError: {
                print::println("Other error occurred loading everything!");
                break;
            }
            case Everything::LoadStatus::UACRejected: {
                print::println("UAC Prompt rejected!");
                break;
            }
        }
        // closed when out of scope
    }
    #endif
}
