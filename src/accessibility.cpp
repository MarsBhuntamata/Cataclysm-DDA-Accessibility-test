#include "accessibility.h"

#include <memory>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "catacharset.h"
#endif

namespace
{

#if defined(_WIN32)
class nvda_controller_backend : public speech_backend
{
    public:
        ~nvda_controller_backend() override {
            if( library != nullptr ) {
                FreeLibrary( library );
            }
        }

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

            const wchar_t *library_names[] = {
                L"nvdaControllerClient.dll",
                sizeof( void * ) == 8 ? L"nvdaControllerClient64.dll" : L"nvdaControllerClient32.dll"
            };

            for( const wchar_t *library_name : library_names ) {
                library = LoadLibraryW( ( library_path + library_name ).c_str() );
                if( library != nullptr ) {
                    break;
                }
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
                FreeLibrary( library );
                library = nullptr;
                return false;
            }
            return true;
        }

        void speak( const narration_request &request ) override
        {
            if( !is_running() ) {
                return;
            }
            if( request.interrupt ) {
                cancel_speech();
            }
            const std::wstring text = utf8_to_wstr( request.text );
            speak_text( text.c_str() );
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
                const std::wstring wide_text = utf8_to_wstr( text );
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
        backend->speak( request );
    }
}

bool narration_service::repeat_last()
{
    if( !has_last_request ) {
        return false;
    }

    if( backend != nullptr ) {
        backend->speak( last_request );
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
        backend.reset();
        get_narration_service().set_backend( nullptr );
        return false;
    }
    get_narration_service().set_backend( backend.get() );
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
    get_nvda_backend().reset();
#endif
}
