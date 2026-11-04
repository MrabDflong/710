#pragma once

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

// Procedural terminal effects and quiet offshore-rig ambience.
enum class SoundCue {
    Select,
    Place,
    Remove,
    Error,
    Verified,
    EngineeringMerit,
    Discovery
};

class TerminalAudio {
public:
    TerminalAudio() = default;
    TerminalAudio(const TerminalAudio&) = delete;
    TerminalAudio& operator=(const TerminalAudio&) = delete;

    ~TerminalAudio() {
        if (ambienceStream_) {
            SDL_DestroyAudioStream(ambienceStream_);
        }
        if (effectsStream_) {
            SDL_DestroyAudioStream(effectsStream_);
        }
        if (audioInitialised_) {
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
        }
    }

    [[nodiscard]] bool enabled() const noexcept {
        return enabled_;
    }

    [[nodiscard]] float effectsVolume() const noexcept {
        return effectsVolume_;
    }

    [[nodiscard]] float ambienceVolume() const noexcept {
        return ambienceVolume_;
    }

    void setEnabled(bool enabled) {
        if (enabled_ == enabled) {
            return;
        }

        enabled_ = enabled;
        applyGains();

        // Do not leave old sounds waiting to play after unmuting.
        if (!enabled_) {
            if (effectsStream_) {
                SDL_ClearAudioStream(effectsStream_);
            }
            if (ambienceStream_) {
                SDL_ClearAudioStream(ambienceStream_);
            }
            ambienceFadeFrames_ = 0;
            resetCreakSchedule();
        }
    }

    void setEffectsVolume(float volume) {
        effectsVolume_ = std::clamp(volume, 0.0f, 1.0f);
        applyGains();
    }

    void setAmbienceVolume(float volume) {
        ambienceVolume_ = std::clamp(volume, 0.0f, 1.0f);
        applyGains();

        if (ambienceVolume_ == 0.0f && ambienceStream_) {
            SDL_ClearAudioStream(ambienceStream_);
            ambienceFadeFrames_ = 0;
            resetCreakSchedule();
        }
    }

    // Call once per game frame.
    void update() {
        if (!enabled_ || ambienceVolume_ <= 0.0f || !ensureAmbience()) {
            return;
        }

        constexpr int framesPerChunk = kSampleRate / 10; // 100 ms
        constexpr int targetBytes = (kSampleRate / 4) * sizeof(float); // 250 ms

        int queuedBytes = SDL_GetAudioStreamQueued(ambienceStream_);
        if (queuedBytes < 0) {
            SDL_Log("710: Cannot query ambience stream: %s", SDL_GetError());
            disableAmbience();
            return;
        }

        // Load audio clips once. Missing files do not stop the background hum or game.
        if (!creaksLoadAttempted_) {
            loadCreakClips();
        }

        while (queuedBytes < targetBytes) {
            std::array<float, framesPerChunk> samples{};

            for (float& sample : samples) {
                const double t = static_cast<double>(ambienceFrame_) / static_cast<double>(kSampleRate);

                constexpr double twoPi = 6.283185307179586;

                // Electrical and mechanical hum with gentle drifting.
                const double pulse = 0.78 + 0.22 * std::sin(twoPi * 0.13 * t);
                const double hum = pulse * (
                    0.024 * std::sin(twoPi * 56.0 * t) +
                    0.010 * std::sin(twoPi * 112.0 * t) +
                    0.006 * std::sin(twoPi * 173.0 * t));

                const double distantRumble =
                    0.007 * std::sin(twoPi * 29.0 * t) *
                    (0.6 + 0.4 * std::sin(twoPi * 0.071 * t));

                // Structural creaks.
                const float creak = nextCreakSample();

                // Fade in when sound starts or resumes after mute.
                constexpr std::uint64_t fadeFrames = kSampleRate / 4;
                const double fade = std::min(1.0, static_cast<double>(ambienceFadeFrames_) / static_cast<double>(fadeFrames));

                sample = static_cast<float>((hum + distantRumble + creak) * fade);

                ++ambienceFrame_;
                if (ambienceFadeFrames_ < fadeFrames) {
                    ++ambienceFadeFrames_;
                }
            }

            const int bytes = static_cast<int>(samples.size() * sizeof(float));

            if (!SDL_PutAudioStreamData(
                ambienceStream_, samples.data(), bytes)) {
                SDL_Log("710: Cannot queue ambience: %s", SDL_GetError());
                disableAmbience();
                return;
            }

            queuedBytes += bytes;
        }
    }

    void play(SoundCue cue) {
        if (!enabled_ || effectsVolume_ <= 0.0f || !ensureEffects()) {
            return;
        }

        struct Note {
            float frequency;
            float duration;
        };

        std::array<Note, 6> notes{};
        std::size_t count = 0;

        switch (cue) {
        case SoundCue::Select:
            notes[0] = { 720.0f, 0.045f };
            count = 1;
            break;
        case SoundCue::Place:
            notes[0] = { 510.0f, 0.045f };
            notes[1] = { 670.0f, 0.060f };
            count = 2;
            break;
        case SoundCue::Remove:
            notes[0] = { 440.0f, 0.040f };
            notes[1] = { 310.0f, 0.055f };
            count = 2;
            break;
        case SoundCue::Error:
            notes[0] = { 240.0f, 0.105f };
            notes[1] = { 175.0f, 0.135f };
            count = 2;
            break;
        case SoundCue::Verified:
            notes[0] = { 480.0f, 0.065f };
            notes[1] = { 640.0f, 0.070f };
            notes[2] = { 850.0f, 0.105f };
            count = 3;
            break;
        case SoundCue::EngineeringMerit:
            notes[0] = { 480.0f, 0.065f };
            notes[1] = { 640.0f, 0.070f };
            notes[2] = { 850.0f, 0.105f };
            notes[3] = { 1010.0f, 0.085f };
            notes[4] = { 1250.0f, 0.095f };
            notes[5] = { 1540.0f, 0.155f };
            count = 6;
            break;
        case SoundCue::Discovery:
            notes[0] = { 360.0f, 0.115f };
            notes[1] = { 540.0f, 0.115f };
            notes[2] = { 720.0f, 0.155f };
            count = 3;
            break;
        }

        constexpr float twoPi = 6.2831853071795864769f;
        constexpr float amplitude = 0.12f;
        constexpr float gapSeconds = 0.012f;
        std::vector<float> samples;
        samples.reserve(kSampleRate);

        for (std::size_t i = 0; i < count; ++i) {
            const Note note = notes[i];
            const int frameCount = static_cast<int>(note.duration * static_cast<float>(kSampleRate));

            for (int frame = 0; frame < frameCount; ++frame) {
                const float time = static_cast<float>(frame) / static_cast<float>(kSampleRate);
                const float attack = std::min(1.0f, time / 0.006f);
                const float release = std::min(1.0f, (note.duration - time) / 0.022f);
                const float envelope = std::max(0.0f, std::min(attack, release));

                const float phase = twoPi * note.frequency * time;
                const float waveform = 0.82f * std::sin(phase) + 0.18f * std::sin(2.0f * phase);

                samples.push_back(amplitude * envelope * waveform);
            }

            if (i + 1 < count) {
                const std::size_t silenceFrames =
                    static_cast<std::size_t>(gapSeconds * kSampleRate);
                samples.insert(samples.end(), silenceFrames, 0.0f);
            }
        }

        // Cancel a preceding unfinished UI cue, not the ambience.
        if (!SDL_ClearAudioStream(effectsStream_)) {
            SDL_Log("710: Cannot clear effects stream: %s", SDL_GetError());
            return;
        }

        const int bytes = static_cast<int>(samples.size() * sizeof(float));
        if (!SDL_PutAudioStreamData(effectsStream_, samples.data(), bytes)) {
            SDL_Log("710: Cannot queue effect: %s", SDL_GetError());
        }
    }

private:
    static constexpr int kSampleRate = 24000;


    void resetCreakSchedule() {
        static std::mt19937 rng{ std::random_device{}() };
        std::uniform_int_distribution<int> dist(17, 42);

        activeCreak_ = -1;
        creakSampleIndex_ = 0;
        // Allow 17-42 seconds of uninterrupted hum when starting/resuming.
        nextCreakFrame_ = ambienceFrame_ + static_cast<std::uint64_t>(dist(rng) * kSampleRate);
    }

    // Every available clip is played once per shuffled cycle. Doesn't repeat any clips.
    void shuffleCreakBag() {
        std::shuffle(creakBag_.begin(), creakBag_.end(), creakRng_);
        if (creakBag_.size() > 1 && static_cast<int>(creakBag_.front()) == lastCreak_) {
            std::swap(creakBag_[0], creakBag_[1]);
        }
        creakBagPosition_ = 0;
    }

    void loadCreakClips() {
        creaksLoadAttempted_ = true;
        creakRng_.seed(static_cast<std::mt19937::result_type>(SDL_GetTicksNS()));

        const char* base = SDL_GetBasePath();
        if (!base) {
            SDL_Log("710: Cannot find audio assets: %s", SDL_GetError());
            return;
        }

        for (std::size_t i = 0; i < creakClips_.size(); ++i) {
            const std::string path = std::string{ base } + "assets/audio/creak_0" + std::to_string(i + 1) + ".wav";

            SDL_AudioSpec spec{};
            Uint8* raw = nullptr;
            Uint32 bytes = 0;

            if (!SDL_LoadWAV(path.c_str(), &spec, &raw, &bytes)) {
                SDL_Log("710: Missing creak clip %s: %s", path.c_str(), SDL_GetError());
                continue;
            }

            // Supplied WAVs are mono 24 kHz IEEE float PCM.
            if (spec.freq == kSampleRate && spec.channels == 1 && spec.format == SDL_AUDIO_F32 && bytes > 0 && bytes % sizeof(float) == 0) {
                auto& samples = creakClips_[i];
                samples.resize(bytes / sizeof(float));
                std::memcpy(samples.data(), raw, bytes);
                creakBag_.push_back(i);
            }
            else {
                SDL_Log("710: Unexpected WAV format for %s", path.c_str());
            }

            SDL_free(raw);
        }

        if (!creakBag_.empty()) {
            shuffleCreakBag();
        }
    }

    [[nodiscard]] float nextCreakSample() {
        // Start the next sound after silence interval has elapsed.
        if (activeCreak_ < 0 &&
            !creakBag_.empty() &&
            ambienceFrame_ >= nextCreakFrame_) {
            if (creakBagPosition_ >= creakBag_.size()) {
                shuffleCreakBag();
            }

            const auto selected = creakBag_[creakBagPosition_++];
            activeCreak_ = static_cast<int>(selected);
            lastCreak_ = activeCreak_;
            creakSampleIndex_ = 0;

            // Very slight volume variation for a natural environment.
            activeCreakGain_ = 0.18f + 0.01f * static_cast<float>(creakRng_() % 7);
        }

        if (activeCreak_ < 0) {
            return 0.0f;
        }

        const auto& clip = creakClips_[
            static_cast<std::size_t>(activeCreak_)];

        const float result = clip[creakSampleIndex_++] * activeCreakGain_;

        if (creakSampleIndex_ >= clip.size()) {
            activeCreak_ = -1;
            creakSampleIndex_ = 0;

            // Random silence between clips.
            std::uniform_int_distribution<std::uint64_t> dist(17, 42);
            const std::uint64_t gapSeconds = dist(creakRng_);
            nextCreakFrame_ = ambienceFrame_ + gapSeconds * kSampleRate;
        }

        return result;
    }

    [[nodiscard]] bool ensureEffects() {
        if (effectsStream_) {
            return true;
        }
        if (audioUnavailable_) {
            return false;
        }

        if (!audioInitialised_) {
            if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
                SDL_Log("710: Audio unavailable: %s", SDL_GetError());
                audioUnavailable_ = true;
                return false;
            }
            audioInitialised_ = true;
        }

        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_F32;
        spec.channels = 1;
        spec.freq = kSampleRate;

        effectsStream_ = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);

        if (!effectsStream_ || !SDL_ResumeAudioStreamDevice(effectsStream_)) {
            SDL_Log("710: Effects unavailable: %s", SDL_GetError());
            if (effectsStream_) {
                SDL_DestroyAudioStream(effectsStream_);
                effectsStream_ = nullptr;
            }
            audioUnavailable_ = true;
            return false;
        }

        applyGains();
        return true;
    }

    [[nodiscard]] bool ensureAmbience() {
        if (ambienceStream_) {
            return true;
        }
        if (ambienceUnavailable_ || !ensureEffects()) {
            return false;
        }

        SDL_AudioSpec spec{};
        spec.format = SDL_AUDIO_F32;
        spec.channels = 1;
        spec.freq = kSampleRate;

        ambienceStream_ = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);

        if (!ambienceStream_ || !SDL_ResumeAudioStreamDevice(ambienceStream_)) {
            SDL_Log("710: Ambience unavailable: %s", SDL_GetError());
            disableAmbience();
            return false;
        }

        applyGains();
        return true;
    }

    void disableAmbience() {
        if (ambienceStream_) {
            SDL_DestroyAudioStream(ambienceStream_);
            ambienceStream_ = nullptr;
        }
        ambienceUnavailable_ = true;
    }

    void applyGains() {
        if (effectsStream_ && !SDL_SetAudioStreamGain(
            effectsStream_, enabled_ ? effectsVolume_ : 0.0f)) {
            SDL_Log("710: Cannot set effect volume: %s", SDL_GetError());
        }
        if (ambienceStream_ && !SDL_SetAudioStreamGain(
            ambienceStream_, enabled_ ? ambienceVolume_ : 0.0f)) {
            SDL_Log("710: Cannot set ambience volume: %s", SDL_GetError());
        }
    }

    SDL_AudioStream* effectsStream_{ nullptr };
    SDL_AudioStream* ambienceStream_{ nullptr };
    bool audioInitialised_{ false };
    bool audioUnavailable_{ false };
    bool ambienceUnavailable_{ false };
    bool enabled_{ true };
    float effectsVolume_{ 0.8f };
    float ambienceVolume_{ 0.45f };
    std::uint64_t ambienceFrame_{ 0 };
    std::uint64_t ambienceFadeFrames_{ 0 };

    // Recorded creaks and their kinda shuffled playback sequence.
    std::array<std::vector<float>, 8> creakClips_{};
    std::vector<std::size_t> creakBag_{};
    std::mt19937 creakRng_{};
    std::size_t creakBagPosition_{ 0 };
    std::size_t creakSampleIndex_{ 0 };
    std::uint64_t nextCreakFrame_{ 20ULL * kSampleRate };
    int activeCreak_{ -1 };
    int lastCreak_{ -1 };
    float activeCreakGain_{ 0.2f };
    bool creaksLoadAttempted_{ false };
};

