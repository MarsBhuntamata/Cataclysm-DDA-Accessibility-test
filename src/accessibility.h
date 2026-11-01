#pragma once
#ifndef CATA_SRC_ACCESSIBILITY_H
#define CATA_SRC_ACCESSIBILITY_H

#include <string>

/// Semantic reason for an announcement.  Presentation backends may use this
/// to apply different speech policies without needing to know about game UI.
enum class narration_event {
    ui_opened,
    ui_closed,
    selection_changed,
    item_description_opened,
    item_description_changed,
    status_changed,
    game_message,
    movement,
    contextual_help
};

/// A request to present meaningful game information to the player.
struct narration_request {
    narration_event event = narration_event::game_message;
    std::string text;
    /// If true, the backend should interrupt speech already in progress.
    bool interrupt = false;
};

/// Platform-specific speech and braille implementations live behind this
/// interface.  Game UI code must not call screen-reader APIs directly.
class speech_backend
{
    public:
        virtual ~speech_backend() = default;

        virtual void speak( const narration_request &request ) = 0;
        virtual void cancel() = 0;
};

/// Coordinates narration independently of any particular screen reader.
class narration_service
{
    public:
        /// The backend is non-owning.  It must outlive this service's use of it.
        void set_backend( speech_backend *backend );

        /// Submit an announcement.  The most recent non-empty request is
        /// retained so it can be repeated even when no backend is connected.
        void announce( const narration_request &request );

        /// Repeat the most recent non-empty announcement.  Returns false when
        /// there is no announcement to repeat.
        bool repeat_last();

        /// Stop current backend speech, if a backend is connected.
        void cancel();

        bool has_last_announcement() const;

    private:
        speech_backend *backend = nullptr;
        narration_request last_request;
        bool has_last_request = false;
};

/// Access the process-wide narration service.
narration_service &get_narration_service();

#endif // CATA_SRC_ACCESSIBILITY_H
