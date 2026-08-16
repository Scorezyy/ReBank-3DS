#include "MusicPlayer.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

MusicPlayer::~MusicPlayer() {
    stop();
}

bool MusicPlayer::play(const char* path) {
    stop();
    if (R_FAILED(ndspInit())) {
        return false;
    }
    ndspReady_ = true;

    FILE* file = std::fopen(path, "rb");
    if (!file) {
        stop();
        return false;
    }
    if (ov_open(file, &vorbis_, nullptr, 0) < 0) {
        std::fclose(file);
        stop();
        return false;
    }
    streamReady_ = true;

    const vorbis_info* info = ov_info(&vorbis_, -1);
    if (!info || info->channels < 1 || info->channels > 2) {
        stop();
        return false;
    }
    channels_ = info->channels;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(Channel);
    ndspChnSetInterp(Channel, NDSP_INTERP_LINEAR);
    ndspChnSetRate(Channel, static_cast<float>(info->rate));
    ndspChnSetFormat(Channel, channels_ == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);

    for (std::size_t index = 0; index < BufferCount; ++index) {
        audioData_[index] = linearAlloc(BufferSize);
        if (!audioData_[index] || !fill(index)) {
            stop();
            return false;
        }
        ndspChnWaveBufAdd(Channel, &waveBuffers_[index]);
    }
    running_.store(true, std::memory_order_release);
    worker_ = threadCreate(streamWorker, this, 32 * 1024, 0x2F, -2, false);
    if (!worker_) {
        running_.store(false, std::memory_order_release);
        stop();
        return false;
    }
    return true;
}

void MusicPlayer::update() {
}

void MusicPlayer::stop() {
    running_.store(false, std::memory_order_release);
    if (worker_) {
        threadJoin(worker_, U64_MAX);
        threadFree(worker_);
        worker_ = nullptr;
    }
    if (ndspReady_) {
        ndspChnReset(Channel);
    }
    if (streamReady_) {
        ov_clear(&vorbis_);
        streamReady_ = false;
    }
    for (void*& data : audioData_) {
        if (data) {
            linearFree(data);
            data = nullptr;
        }
    }
    waveBuffers_.fill({});
    if (ndspReady_) {
        ndspExit();
        ndspReady_ = false;
    }
    channels_ = 0;
}

void MusicPlayer::streamWorker(void* argument) {
    static_cast<MusicPlayer*>(argument)->streamLoop();
}

void MusicPlayer::streamLoop() {
    while (running_.load(std::memory_order_acquire)) {
        bool filled = false;
        for (std::size_t index = 0; index < BufferCount; ++index) {
            if (waveBuffers_[index].status != NDSP_WBUF_DONE) {
                continue;
            }
            if (!fill(index)) {
                running_.store(false, std::memory_order_release);
                break;
            }
            ndspChnWaveBufAdd(Channel, &waveBuffers_[index]);
            filled = true;
        }
        svcSleepThread(filled ? 1000000LL : 4000000LL);
    }
}

bool MusicPlayer::fill(std::size_t index) {
    auto* destination = static_cast<char*>(audioData_[index]);
    std::size_t written = 0;
    int bitstream = 0;
    int restartCount = 0;
    while (written < BufferSize) {
        const long result = ov_read(&vorbis_, destination + written, BufferSize - written, &bitstream);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
        } else if (result == 0 && restartCount++ == 0 && ov_raw_seek(&vorbis_, 0) == 0) {
            continue;
        } else {
            break;
        }
    }
    if (written == 0) {
        return false;
    }

    ndspWaveBuf& waveBuffer = waveBuffers_[index];
    std::memset(&waveBuffer, 0, sizeof(waveBuffer));
    waveBuffer.data_vaddr = audioData_[index];
    waveBuffer.nsamples = written / (static_cast<std::size_t>(channels_) * sizeof(std::int16_t));
    DSP_FlushDataCache(audioData_[index], written);
    return true;
}