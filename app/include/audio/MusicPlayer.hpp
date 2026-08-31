#pragma once

#include <3ds.h>
#include <tremor/ivorbisfile.h>

#include <atomic>
#include <array>
#include <cstddef>

class MusicPlayer {
public:
    MusicPlayer() = default;
    ~MusicPlayer();

    bool start(const char* normalPath, const char* virtualConsolePath);
    void setActive(bool virtualConsole);
    void stop();

private:
    static constexpr std::size_t BufferCount = 8;
    static constexpr std::size_t BufferSize = 64 * 1024;

    struct Voice {
        int channelId = 0;
        OggVorbis_File vorbis{};
        std::array<ndspWaveBuf, BufferCount> waveBuffers{};
        std::array<void*, BufferCount> audioData{};
        int channels = 0;
        Thread worker = nullptr;
        std::atomic<bool> running{false};
        bool streamReady = false;
    };

    bool openVoice(Voice& voice, const char* path);
    void closeVoice(Voice& voice);
    static void streamWorker(void* argument);
    static void streamLoop(Voice& voice);
    static bool fill(Voice& voice, std::size_t index);
    void applyMix();

    Voice normalVoice_{0};
    Voice virtualConsoleVoice_{1};
    bool ndspReady_ = false;
    bool active_ = false;
};
