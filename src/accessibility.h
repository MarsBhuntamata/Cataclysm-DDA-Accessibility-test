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
        /// Optional braille output. Backends without braille support may ignore it.
        virtual void braille( const std::string &text ) {
            static_cast<void>( text );
        }
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
        void braille( const std::string &text );

        bool has_last_announcement() const;

    private:
        speech_backend *backend = nullptr;
        narration_request last_request;
        bool has_last_request = false;
};

/// Access the process-wide narration service.
narration_service &get_narration_service();

/// Load an available platform speech backend. Returns true when one is ready.
bool initialize_accessibility();

/// Detach and release the active platform speech backend.
void shutdown_accessibility();

/// Persist startup and shutdown checkpoints to help diagnose Windows crashes.
void record_accessibility_checkpoint( const std::string &stage );

// Low-frequency look-mode performance records, independent of NVDA speech.
// On Windows, writes look-profiler.txt next to the executable.
void record_look_profiler_sample( const std::string &message );

#endif // CATA_SRC_ACCESSIBILITY_H
