#include "accessibility.h"

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

bool narration_service::has_last_announcement() const
{
    return has_last_request;
}

narration_service &get_narration_service()
{
    static narration_service service;
    return service;
}
