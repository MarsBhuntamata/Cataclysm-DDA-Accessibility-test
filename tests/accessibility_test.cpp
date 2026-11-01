#include "accessibility.h"

#include <string>
#include <vector>

#include "cata_catch.h"

namespace
{

class recording_speech_backend : public speech_backend
{
    public:
        void speak( const narration_request &request ) override {
            requests.push_back( request );
        }

        void cancel() override {
            ++cancel_count;
        }

        std::vector<narration_request> requests;
        int cancel_count = 0;
};

} // namespace

TEST_CASE( "narration service remembers and repeats announcements", "[accessibility]" )
{
    narration_service service;
    recording_speech_backend backend;
    service.set_backend( &backend );

    CHECK_FALSE( service.has_last_announcement() );
    CHECK_FALSE( service.repeat_last() );

    const narration_request request{
        narration_event::item_description_opened, "A sturdy wooden club.", false
    };
    service.announce( request );

    REQUIRE( backend.requests.size() == 1 );
    CHECK( backend.requests.back().text == "A sturdy wooden club." );
    CHECK( backend.requests.back().event == narration_event::item_description_opened );
    CHECK( service.has_last_announcement() );

    CHECK( service.repeat_last() );
    REQUIRE( backend.requests.size() == 2 );
    CHECK( backend.requests.back().text == "A sturdy wooden club." );
    CHECK( backend.requests.back().event == narration_event::item_description_opened );

    service.cancel();
    CHECK( backend.cancel_count == 1 );
}

TEST_CASE( "empty narration requests are ignored", "[accessibility]" )
{
    narration_service service;
    recording_speech_backend backend;
    service.set_backend( &backend );

    service.announce( narration_request{ narration_event::game_message, "", false } );

    CHECK_FALSE( service.has_last_announcement() );
    CHECK( backend.requests.empty() );
}

TEST_CASE( "narration service safely accepts announcements without a backend", "[accessibility]" )
{
    narration_service service;

    service.announce( narration_request{ narration_event::game_message, "The door is locked.", false } );

    CHECK( service.has_last_announcement() );
    CHECK( service.repeat_last() );
}

