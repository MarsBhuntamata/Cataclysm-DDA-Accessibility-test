#include "accessibility.h"

#include <chrono>\n#include <cwctype>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>

#include <filesystem>
#include <fstream>

namespace
{

std::ofstream *accessibility_diagnostic_file()
{
    // Intentionally keep this stream alive until process termination.  This
    // avoids running C++ stream destruction during the game's fragile shutdown.
    static std::ofstream *file = []() {
        std::vector<wchar_t> executable_path( 32768, L'\0' );
        const DWORD path_length = GetModuleFileNameW( nullptr, executable_path.data(),
                                    static_cast<DWORD>( executable_path.size() ) );
        if( path_length == 0 || path_length >= executable_path.size() ) {
            OutputDebugStringA( "[accessibility] Could not determine executable path for diagnostics.\n" );
            return static_cast<std::ofstream *>( nullptr );
        }

        const std::filesystem::path executable( std::wstring( executable_path.data(), path_length ) );
        const std::filesystem::path log_path = executable.parent_path() /
                                               L"nvda-accessibility-diagnostics.txt";
        std::ofstream *output = new std::ofstream( log_path, std::ios::out | std::ios::trunc );
        if( *output ) {
            *output << "CDDA NVDA accessibility diagnostics\n";
            *output << "Diagnostic file is beside cataclysm-tiles.exe.\n";
            output->flush();
        } else {
            delete output;
            OutputDebugStringA( "[accessibility] Could not create diagnostic file.\n" );
            return static_cast<std::ofstream *>( nullptr );
        }
        return output;
    }();
    return file;
}

void log_accessibility_diagnostic( const std::string &message )
{
    std::ofstream *file = accessibility_diagnostic_file();
    if( file != nullptr && *file ) {
        *file << message << std::endl;
    } else {
        OutputDebugStringA( ( "[accessibility] " + message + "\n" ).c_str() );
    }
}

std::wstring utf8_to_wide( const std::string &text )
{
    if( text.empty() ) {
        return std::wstring();
    }
    const int required = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                         static_cast<int>( text.size() ), nullptr, 0 );
    if( required <= 0 ) {
        return std::wstring();
    }
    std::wstring result( static_cast<size_t>( required ), L'\0' );
    const int converted = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>( text.size() ), result.data(), required );
    if( converted != required ) {
        return std::wstring();
    }
    return result;
}

class nvda_controller_backend : public speech_backend
{
    public:
        // Keep the controller DLL loaded until Windows tears down the process.
        // The DLL is intentionally not unloaded during game shutdown.
        ~nvda_controller_backend() override = default;

        bool initialize()
        {
            log_accessibility_diagnostic( "Backend initialization started." );
            std::vector<wchar_t> executable_path( 32768, L'\0' );
            const DWORD path_length = GetModuleFileNameW( nullptr, executable_path.data(),
                                        static_cast<DWORD>( executable_path.size() ) );
            if( path_length == 0 || path_length >= executable_path.size() ) {
                log_accessibility_diagnostic( "FAIL: GetModuleFileNameW failed; Windows error " +
                                              std::to_string( GetLastError() ) );
                return false;
            }

            std::wstring library_directory( executable_path.data(), path_length );
            const size_t separator = library_directory.find_last_of( L"\\/" );
            if( separator == std::wstring::npos ) {
                log_accessibility_diagnostic( "FAIL: executable path had no directory separator." );
                return false;
            }
            library_directory.resize( separator + 1 );

            const wchar_t *legacy_library_name = sizeof( void * ) == 8 ?
                                                    L"nvdaControllerClient64.dll" :
                                                    L"nvdaControllerClient32.dll";
            const wchar_t *library_names[] = {
                L"nvdaControllerClient.dll",
                legacy_library_name
            };

            for( const wchar_t *library_name : library_names ) {
                const std::wstring full_path = library_directory + library_name;
                library = LoadLibraryW( full_path.c_str() );
                if( library != nullptr ) {
                    log_accessibility_diagnostic( std::string( "Loaded controller DLL: " ) +
                                                  ( sizeof( void * ) == 8 ? "64-bit controller library." :
                                                    "32-bit controller library." ) );
                    break;
                }
                const DWORD load_error = GetLastError();
                log_accessibility_diagnostic( "Could not load controller DLL candidate; Windows error " +
                                              std::to_string( load_error ) );
            }
            if( library == nullptr ) {
                log_accessibility_diagnostic( "FAIL: no NVDA controller DLL was loaded." );
                return false;
            }

            test_if_running = reinterpret_cast<test_if_running_fn>(
                                  GetProcAddress( library, "nvdaController_testIfRunning" ) );
            speak_text = reinterpret_cast<speak_text_fn>(
                             GetProcAddress( library, "nvdaController_speakText" ) );
            cancel_speech = reinterpret_cast<cancel_speech_fn>(
                                GetProcAddress( library, "nvdaController_cancelSpeech" ) );
            braille_message = reinterpret_cast<braille_message_fn>(
                                  GetProcAddress( library, "nvdaController_brailleMessage" ) );

            log_accessibility_diagnostic( std::string( "API exports: testIfRunning=" ) +
                                          ( test_if_running ? "yes" : "NO" ) +
                                          ", speakText=" + ( speak_text ? "yes" : "NO" ) +
                                          ", cancelSpeech=" + ( cancel_speech ? "yes" : "NO" ) +
                                          ", brailleMessage=" + ( braille_message ? "yes" : "no" ) );
            if( test_if_running == nullptr || speak_text == nullptr || cancel_speech == nullptr ) {
                log_accessibility_diagnostic( "FAIL: controller DLL is missing a required API export." );
                return false;
            }

            const unsigned long running_result = test_if_running();
            log_accessibility_diagnostic( "nvdaController_testIfRunning returned " +
                                          std::to_string( running_result ) +
                                          ( running_result == 0 ? " (NVDA available)." : " (NVDA unavailable)." ) );
            if( running_result != 0 ) {
                return false;
            }
            log_accessibility_diagnostic( "PASS: NVDA controller backend is ready." );
            return true;
        }

        void speak( const narration_request &request ) override
        {
            log_accessibility_diagnostic( "Speech requested: event=" +
                                          std::to_string( static_cast<int>( request.event ) ) +
                                          ", UTF-8 bytes=" + std::to_string( request.text.size() ) +
                                          ", interrupt=" + ( request.interrupt ? "yes" : "no" ) );
            const unsigned long running_result = test_if_running == nullptr ? 1 : test_if_running();
            log_accessibility_diagnostic( "Speech-time testIfRunning returned " +
                                          std::to_string( running_result ) );
            if( running_result != 0 ) {
                log_accessibility_diagnostic( "FAIL: speech skipped because NVDA is unavailable." );
                return;
            }
            if( request.interrupt ) {
                const unsigned long cancel_result = cancel_speech();
                log_accessibility_diagnostic( "nvdaController_cancelSpeech returned " +
                                              std::to_string( cancel_result ) );
            }
            const std::wstring text = utf8_to_wide( request.text );
            if( text.empty() && !request.text.empty() ) {
                log_accessibility_diagnostic( "FAIL: UTF-8 to UTF-16 conversion failed." );
                return;
            }
            const unsigned long speak_result = speak_text( text.c_str() );
            log_accessibility_diagnostic( "nvdaController_speakText returned " +
                                          std::to_string( speak_result ) +
                                          ( speak_result == 0 ? " (success)." : " (failure)." ) );
        }

        void cancel() override
        {
            if( is_running() ) {
                const unsigned long result = cancel_speech();
                log_accessibility_diagnostic( "Cancel request returned " + std::to_string( result ) );
            } else {
                log_accessibility_diagnostic( "Cancel request skipped; NVDA is unavailable." );
            }
        }

        void braille( const std::string &text ) override
        {
            if( is_running() && braille_message != nullptr ) {
                const std::wstring wide_text = utf8_to_wide( text );
                const unsigned long result = braille_message( wide_text.c_str() );
                log_accessibility_diagnostic( "Braille request returned " + std::to_string( result ) );
            }
        }

    private:
        using test_if_running_fn = unsigned long ( __stdcall * )( void );
        using speak_text_fn = unsigned long ( __stdcall * )( const wchar_t * );
        using cancel_speech_fn = unsigned long ( __stdcall * )( void );
        using braille_message_fn = unsigned long ( __stdcall * )( const wchar_t * );

        HMODULE library = nullptr;
        test_if_running_fn test_if_running = nullptr;
        speak_text_fn speak_text = nullptr;
        cancel_speech_fn cancel_speech = nullptr;
        braille_message_fn braille_message = nullptr;

        bool is_running() const
        {
            return test_if_running != nullptr && test_if_running() == 0;
        }
};

// Optional non-NVDA screen-reader bridge.  The game never sends narration to
// both NVDA Controller and Tolk: exactly one backend attaches to the service.
// Tolk.dll is loaded at runtime, so the NVDA-only package still runs without it.
class tolk_screenreader_backend : public speech_backend
{
    public:
        bool initialize()
        {
            std::vector<wchar_t> executable_path( 32768, L'\\0' );
            const DWORD length = GetModuleFileNameW( nullptr, executable_path.data(),
                                 static_cast<DWORD>( executable_path.size() ) );
            if( length == 0 || length >= executable_path.size() ) {
                return false;
            }
            std::wstring path( executable_path.data(), length );
            const size_t separator = path.find_last_of( L"\\\\/" );
            if( separator == std::wstring::npos ) {
                return false;
            }
            path.resize( separator + 1 );
            path += L"Tolk.dll";
            library = LoadLibraryW( path.c_str() );
            if( library == nullptr ) {
                log_accessibility_diagnostic( "Tolk.dll not packaged; non-NVDA output unavailable." );
                return false;
            }

            load = reinterpret_cast<load_fn>( GetProcAddress( library, "Tolk_Load" ) );
            unload = reinterpret_cast<unload_fn>( GetProcAddress( library, "Tolk_Unload" ) );
            try_sapi = reinterpret_cast<try_sapi_fn>( GetProcAddress( library, "Tolk_TrySAPI" ) );
            detect = reinterpret_cast<detect_fn>( GetProcAddress( library, "Tolk_DetectScreenReader" ) );
            output = reinterpret_cast<output_fn>( GetProcAddress( library, "Tolk_Output" ) );
            silence = reinterpret_cast<silence_fn>( GetProcAddress( library, "Tolk_Silence" ) );
            braille_text = reinterpret_cast<braille_fn>( GetProcAddress( library, "Tolk_Braille" ) );
            if( !load || !unload || !try_sapi || !detect || !output || !silence ) {
                log_accessibility_diagnostic( "Tolk.dll lacks required exports." );
                return false;
            }
            try_sapi( false ); // Never silently speak via a second synthesizer.
            load();
            loaded = true;
            const wchar_t *reader = detect();
            if( reader == nullptr || reader[0] == L'\\0' ) {
                log_accessibility_diagnostic( "Tolk detected no active reader." );
                return false;
            }
            std::wstring reader_name( reader );
            std::wstring lowered = reader_name;
            for( wchar_t &ch : lowered ) {
                ch = static_cast<wchar_t>( towlower( ch ) );
            }
            // Do not route NVDA through Tolk, even if the direct client is
            // temporarily unavailable: that could duplicate speech later.
            if( lowered.find( L"nvda" ) != std::wstring::npos ) {
                log_accessibility_diagnostic( "Tolk selected NVDA; refusing duplicate NVDA route." );
                return false;
            }
            log_accessibility_diagnostic( "Tolk attached to a non-NVDA screen reader." );
            return true;
        }

        ~tolk_screenreader_backend() override
        {
            if( loaded && unload != nullptr ) {
                unload();
            }
            // Keep the DLL mapped through Windows process teardown.
        }

        void speak( const narration_request &request ) override
        {
            const std::wstring wide = utf8_to_wide( request.text );
            if( !wide.empty() && output != nullptr ) {
                output( wide.c_str(), request.interrupt );
            }
        }

        void cancel() override
        {
            if( silence != nullptr ) {
                silence();
            }
        }

        void braille( const std::string &text ) override
        {
            if( braille_text != nullptr ) {
                const std::wstring wide = utf8_to_wide( text );
                if( !wide.empty() ) {
                    braille_text( wide.c_str() );
                }
            }
        }

    private:
        using load_fn = void ( __cdecl * )();
        using unload_fn = void ( __cdecl * )();
        using try_sapi_fn = void ( __cdecl * )( bool );
        using detect_fn = const wchar_t *( __cdecl * )();
        using output_fn = bool ( __cdecl * )( const wchar_t *, bool );
        using silence_fn = bool ( __cdecl * )();
        using braille_fn = bool ( __cdecl * )( const wchar_t * );

        HMODULE library = nullptr;
        load_fn load = nullptr;
        unload_fn unload = nullptr;
        try_sapi_fn try_sapi = nullptr;
        detect_fn detect = nullptr;
        output_fn output = nullptr;
        silence_fn silence = nullptr;
        braille_fn braille_text = nullptr;
        bool loaded = false;
};

std::unique_ptr<tolk_screenreader_backend> &get_tolk_backend()
{
    static std::unique_ptr<tolk_screenreader_backend> backend;
    return backend;
}

std::unique_ptr<nvda_controller_backend> &get_nvda_backend()
{
    static std::unique_ptr<nvda_controller_backend> backend;
    return backend;
}

} // namespace
#endif

void narration_service::set_backend( speech_backend *new_backend )
{
    backend = new_backend;
}

void narration_service::announce( const narration_request &request )
{
    if( request.text.empty() ) {
        return;
    }

    // Prevent per-turn status/game-message loops (including temperature effects)
    // from flooding an attached screen reader during long activities.
    // This does not change the game's message log, or suppress menu navigation.
    if( request.event == narration_event::game_message ||
        request.event == narration_event::status_changed ) {
        using clock = std::chrono::steady_clock;
        static std::deque<std::pair<std::string, clock::time_point>> recent_speech;
        const auto now = clock::now();
        const auto silence_window = std::chrono::seconds( 5 );
        while( !recent_speech.empty() && now - recent_speech.front().second > silence_window ) {
            recent_speech.pop_front();
        }
        const std::string key = std::to_string( static_cast<int>( request.event ) ) +
                                ":" + request.text;
        for( auto &previous : recent_speech ) {
            if( previous.first == key ) {
                return;
            }
        }
        recent_speech.emplace_back( key, now );
        if( recent_speech.size() > 64 ) {
            recent_speech.pop_front();
        }
    }

    last_request = request;
    has_last_request = true;

#if defined(_WIN32)
    log_accessibility_diagnostic( "Narration service received event=" +
                                  std::to_string( static_cast<int>( request.event ) ) +
                                  ", UTF-8 bytes=" + std::to_string( request.text.size() ) +
                                  ( backend != nullptr ? "; backend attached." : "; NO backend attached." ) );
#endif
    if( backend != nullptr ) {
        backend->speak( request );
    }
}

bool narration_service::repeat_last()
{
    if( !has_last_request ) {
#if defined(_WIN32)
        log_accessibility_diagnostic( "Repeat requested, but no previous narration exists." );
#endif
        return false;
    }

    if( backend != nullptr ) {
        narration_request repeated = last_request;
        repeated.interrupt = true;
        backend->speak( repeated );
    } else {
#if defined(_WIN32)
        log_accessibility_diagnostic( "Repeat requested, but no speech backend is attached." );
#endif
    }
    return true;
}

void narration_service::cancel()
{
    if( backend != nullptr ) {
        backend->cancel();
    }
}

void narration_service::braille( const std::string &text )
{
    if( backend != nullptr && !text.empty() ) {
        backend->braille( text );
    }
}

bool narration_service::has_last_announcement() const
{
    return has_last_request;
}

narration_service &get_narration_service()
{
    static narration_service service;
    return service;
}

bool initialize_accessibility()
{
#if defined(_WIN32)
    log_accessibility_diagnostic( "Accessibility initialization entry point reached." );
    std::unique_ptr<nvda_controller_backend> &backend = get_nvda_backend();
    backend = std::make_unique<nvda_controller_backend>();
    if( backend->initialize() ) {
        get_narration_service().set_backend( backend.get() );
        log_accessibility_diagnostic( "PASS: NVDA backend initialized and attached to narration service." );
        return true;
    }
    backend.reset();
    log_accessibility_diagnostic( "NVDA unavailable. Trying non-NVDA Tolk backend." );
    std::unique_ptr<tolk_screenreader_backend> &tolk = get_tolk_backend();
    tolk = std::make_unique<tolk_screenreader_backend>();
    if( tolk->initialize() ) {
        get_narration_service().set_backend( tolk.get() );
        log_accessibility_diagnostic( "PASS: non-NVDA Tolk backend initialized." );
        return true;
    }
    tolk.reset();
    get_narration_service().set_backend( nullptr );
    log_accessibility_diagnostic( "No supported screen reader detected." );
    return false;
#else
    get_narration_service().set_backend( nullptr );
    return false;
#endif
}

void record_accessibility_checkpoint( const std::string &stage )
{
#if defined(_WIN32)
    log_accessibility_diagnostic( "LIFECYCLE: " + stage );
#else
    static_cast<void>( stage );
#endif
}

void record_look_profiler_sample( const std::string &message )
{
#if defined(_WIN32)
    // Keep look diagnostics separate from the regular NVDA speech trace.
    static std::ofstream *file = []() -> std::ofstream * {
        std::vector<wchar_t> path( 32768, L'\0' );
        const DWORD len = GetModuleFileNameW( nullptr, path.data(),
                          static_cast<DWORD>( path.size() ) );
        if( len == 0 || len >= path.size() ) {
            return nullptr;
        }
        const std::filesystem::path exe( std::wstring( path.data(), len ) );
        auto *out = new std::ofstream( exe.parent_path() / L"look-profiler.txt",
                                      std::ios::out | std::ios::app );
        if( !*out ) {
            delete out;
            return nullptr;
        }
        *out << "LOOK PROFILER START (new game process)\n";
        out->flush();
        return out;
    }();
    if( file == nullptr || !*file ) {
        return;
    }

    // Dynamic lookup avoids an additional psapi.lib dependency in Windows builds.
    using memory_info_fn = BOOL( WINAPI * )( HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD );
    static memory_info_fn get_memory_info = []() -> memory_info_fn {
        const HMODULE kernel = GetModuleHandleW( L"kernel32.dll" );
        if( kernel == nullptr ) {
            return nullptr;
        }
        return reinterpret_cast<memory_info_fn>(
                   GetProcAddress( kernel, "K32GetProcessMemoryInfo" ) );
    }();
    std::string memory_text = "memory unavailable";
    if( get_memory_info != nullptr ) {
        PROCESS_MEMORY_COUNTERS_EX mem = {};
        mem.cb = sizeof( mem );
        if( get_memory_info( GetCurrentProcess(),
                             reinterpret_cast<PPROCESS_MEMORY_COUNTERS>( &mem ), sizeof( mem ) ) ) {
            memory_text = "working_set_MiB=" + std::to_string( mem.WorkingSetSize / ( 1024 * 1024 ) ) +
                          " private_MiB=" + std::to_string( mem.PrivateUsage / ( 1024 * 1024 ) ) +
                          " peak_working_set_MiB=" + std::to_string( mem.PeakWorkingSetSize / ( 1024 * 1024 ) );
        }
    }
    *file << message << " " << memory_text << '\n';
    file->flush();
#else
    static_cast<void>( message );
#endif
}

void set_look_profiler_active( bool enabled )
{
    static_cast<void>( enabled );
    // Accessed only by the main game's look and render thread.
    extern bool &look_profiler_enabled_storage();
    look_profiler_enabled_storage() = enabled;
}

bool &look_profiler_enabled_storage()
{
    static bool enabled = false;
    return enabled;
}

bool look_profiler_active()
{
    return look_profiler_enabled_storage();
}

void shutdown_accessibility()
{
#if defined(_WIN32)
    log_accessibility_diagnostic( "Accessibility shutdown entry point reached." );
#endif
    get_narration_service().set_backend( nullptr );
#if defined(_WIN32)
    // The controller DLL intentionally remains loaded until process termination.
    get_nvda_backend().reset();
    get_tolk_backend().reset();
    log_accessibility_diagnostic( "Accessibility backend detached; libraries remain mapped." );
#endif
}
