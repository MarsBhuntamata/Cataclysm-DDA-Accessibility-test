#include "accessibility.h"

#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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
    if( !backend->initialize() ) {
        log_accessibility_diagnostic( "FAIL: NVDA backend initialization failed." );
        backend.reset();
        get_narration_service().set_backend( nullptr );
        return false;
    }
    get_narration_service().set_backend( backend.get() );
    log_accessibility_diagnostic( "PASS: NVDA backend initialized and attached to narration service." );
    return true;
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

void shutdown_accessibility()
{
#if defined(_WIN32)
    log_accessibility_diagnostic( "Accessibility shutdown entry point reached." );
#endif
    get_narration_service().set_backend( nullptr );
#if defined(_WIN32)
    // The controller DLL intentionally remains loaded until process termination.
    get_nvda_backend().reset();
    log_accessibility_diagnostic( "Accessibility backend detached; controller DLL remains loaded." );
#endif
}
