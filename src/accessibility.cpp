#include "accessibility.h"

#include "debug.h"

#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#endif

namespace
{

#if defined(_WIN32)
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
        // Unloading it during normal game shutdown can run DLL teardown code while
        // other process-wide objects are already being destroyed.
        ~nvda_controller_backend() override = default;

        bool initialize()
        {
            std::vector<wchar_t> executable_path( 32768, L'\0' );
            const DWORD path_length = GetModuleFileNameW( nullptr, executable_path.data(),
                                        static_cast<DWORD>( executable_path.size() ) );
            if( path_length == 0 || path_length >= executable_path.size() ) {
                return false;
            }

            std::wstring library_path( executable_path.data(), path_length );
            const size_t separator = library_path.find_last_of( L"\\/" );
            if( separator == std::wstring::npos ) {
                return false;
            }
            library_path.resize( separator + 1 );

            const wchar_t *legacy_library_name = sizeof( void * ) == 8 ?
                                                    L"nvdaControllerClient64.dll" :
                                                    L"nvdaControllerClient32.dll";
            const wchar_t *library_names[] = {
                L"nvdaControllerClient.dll",
                legacy_library_name
            };

            for( const wchar_t *library_name : library_names ) {
                library = LoadLibraryW( ( library_path + library_name ).c_str() );
                if( library != nullptr ) {
                    DebugLog( D_INFO, D_MAIN ) << "[accessibility] Loaded NVDA controller DLL: "
                                               << ( sizeof( void * ) == 8 ? "64-bit" : "32-bit" );
                    break;
                }
                DebugLog( D_WARNING, D_MAIN ) << "[accessibility] Could not load "
                                              << ( sizeof( void * ) == 8 ? "nvdaControllerClient.dll or nvdaControllerClient64.dll" :
                                                   "nvdaControllerClient.dll or nvdaControllerClient32.dll" )
                                              << "; Windows error " << GetLastError();
            }
            if( library == nullptr ) {
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

            if( test_if_running == nullptr || speak_text == nullptr || cancel_speech == nullptr ) {
                DebugLog( D_ERROR, D_MAIN ) << "[accessibility] NVDA controller DLL is missing a required API export.";
                return false;
            }
            const unsigned long running_result = test_if_running();
            if( running_result != 0 ) {
                DebugLog( D_WARNING, D_MAIN ) << "[accessibility] NVDA is not reachable; controller API returned "
                                              << running_result << ".";
                return false;
            }
            DebugLog( D_INFO, D_MAIN ) << "[accessibility] NVDA controller API is ready.";
            return true;
        }

        void speak( const narration_request &request ) override
        {
            const unsigned long running_result = test_if_running == nullptr ? 1 : test_if_running();
            if( running_result != 0 ) {
                DebugLog( D_WARNING, D_MAIN ) << "[accessibility] Speech skipped; NVDA test returned "
                                              << running_result << ".";
                return;
            }
            if( request.interrupt ) {
                cancel_speech();
            }
            const std::wstring text = utf8_to_wide( request.text );
            if( text.empty() && !request.text.empty() ) {
                DebugLog( D_ERROR, D_MAIN ) << "[accessibility] UTF-8 to UTF-16 conversion failed.";
                return;
            }
            const unsigned long speak_result = speak_text( text.c_str() );
            if( speak_result != 0 ) {
                DebugLog( D_ERROR, D_MAIN ) << "[accessibility] NVDA speakText returned " << speak_result << ".";
            }
        }

        void cancel() override
        {
            if( is_running() ) {
                cancel_speech();
            }
        }

        void braille( const std::string &text ) override
        {
            if( is_running() && braille_message != nullptr ) {
                const std::wstring wide_text = utf8_to_wide( text );
                braille_message( wide_text.c_str() );
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
#endif

} // namespace

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

    if( backend != nullptr ) {
        DebugLog( D_INFO, D_MAIN ) << "[accessibility] Sending narration event "
                                  << static_cast<int>( request.event ) << " (" << request.text.size()
                                  << " UTF-8 bytes) to the active backend.";
        backend->speak( request );
    } else {
        DebugLog( D_WARNING, D_MAIN ) << "[accessibility] Narration requested but no speech backend is active.";
    }
}

bool narration_service::repeat_last()
{
    if( !has_last_request ) {
        return false;
    }

    if( backend != nullptr ) {
        narration_request repeated = last_request;
        repeated.interrupt = true;
        backend->speak( repeated );
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
    std::unique_ptr<nvda_controller_backend> &backend = get_nvda_backend();
    backend = std::make_unique<nvda_controller_backend>();
    if( !backend->initialize() ) {
        DebugLog( D_ERROR, D_MAIN ) << "[accessibility] NVDA backend initialization failed.";
        backend.reset();
        get_narration_service().set_backend( nullptr );
        return false;
    }
    get_narration_service().set_backend( backend.get() );
    DebugLog( D_INFO, D_MAIN ) << "[accessibility] NVDA backend initialized successfully.";
    return true;
#else
    get_narration_service().set_backend( nullptr );
    return false;
#endif
}

void shutdown_accessibility()
{
    get_narration_service().set_backend( nullptr );
#if defined(_WIN32)
    // The controller DLL intentionally remains loaded until process termination.
    get_nvda_backend().reset();
#endif
}
