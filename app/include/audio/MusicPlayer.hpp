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
    bool play(const char* path);
    void update();
    void stop();

private:
    static constexpr int Channel = 0;
    static constexpr std::size_t BufferCount = 3;
    static constexpr std::size_t BufferSize = 32 * 1024;

    static void streamWorker(void* argument);
    void streamLoop();
    bool fill(std::size_t index);

    OggVorbis_File vorbis_{};
    std::array<ndspWaveBuf, BufferCount> waveBuffers_{};
    std::array<void*, BufferCount> audioData_{};
    int channels_ = 0;
    Thread worker_ = nullptr;
    std::atomic<bool> running_{false};
    bool ndspReady_ = false;
    bool streamReady_ = false;
};