#include "audio/MusicPlayer.hpp"
#include "core/FsGuard.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

MusicPlayer::~MusicPlayer() {
    stop();
}

bool MusicPlayer::openVoice(Voice& voice, const char* path) {
    FILE* file = std::fopen(path, "rb");
    if (!file) {
        return false;
    }
    if (ov_open(file, &voice.vorbis, nullptr, 0) < 0) {
        std::fclose(file);
        return false;
    }
    voice.streamReady = true;

    const vorbis_info* info = ov_info(&voice.vorbis, -1);
    if (!info || info->channels < 1 || info->channels > 2) {
        return false;
    }
    voice.channels = info->channels;

    ndspChnReset(voice.channelId);
    ndspChnSetInterp(voice.channelId, NDSP_INTERP_LINEAR);
    ndspChnSetRate(voice.channelId, static_cast<float>(info->rate));
    ndspChnSetFormat(voice.channelId,
        voice.channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    float silentMix[12]{};
    ndspChnSetMix(voice.channelId, silentMix);

    for (std::size_t index = 0; index < BufferCount; ++index) {
        voice.audioData[index] = linearAlloc(BufferSize);
        if (!voice.audioData[index] || !fill(voice, index)) {
            return false;
        }
        ndspChnWaveBufAdd(voice.channelId, &voice.waveBuffers[index]);
    }
    voice.running.store(true, std::memory_order_release);
    voice.worker = threadCreate(&MusicPlayer::streamWorker, &voice, 32 * 1024, 0x2F, -2, false);
    return voice.worker != nullptr;
}

void MusicPlayer::closeVoice(Voice& voice) {
    voice.running.store(false, std::memory_order_release);
    if (voice.worker) {
        threadJoin(voice.worker, U64_MAX);
        threadFree(voice.worker);
        voice.worker = nullptr;
    }
    if (voice.streamReady) {
        ov_clear(&voice.vorbis);
        voice.streamReady = false;
    }
    for (void*& data : voice.audioData) {
        if (data) {
            linearFree(data);
            data = nullptr;
        }
    }
    voice.waveBuffers.fill({});
    voice.channels = 0;
}

bool MusicPlayer::start(const char* normalPath, const char* virtualConsolePath) {
    stop();
    if (R_FAILED(ndspInit())) {
        return false;
    }
    ndspReady_ = true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    if (!openVoice(normalVoice_, normalPath) || !openVoice(virtualConsoleVoice_, virtualConsolePath)) {
        stop();
        return false;
    }
    active_ = false;
    applyMix();
    return true;
}

void MusicPlayer::setActive(bool virtualConsole) {
    if (!ndspReady_) {
        return;
    }
    active_ = virtualConsole;
    applyMix();
}

void MusicPlayer::applyMix() {
    float normalMix[12]{};
    float virtualConsoleMix[12]{};
    normalMix[0] = normalMix[1] = active_ ? 0.0F : 1.0F;
    virtualConsoleMix[0] = virtualConsoleMix[1] = active_ ? 1.0F : 0.0F;
    ndspChnSetMix(normalVoice_.channelId, normalMix);
    ndspChnSetMix(virtualConsoleVoice_.channelId, virtualConsoleMix);
}

void MusicPlayer::stop() {
    closeVoice(normalVoice_);
    closeVoice(virtualConsoleVoice_);
    if (ndspReady_) {
        ndspExit();
        ndspReady_ = false;
    }
}

void MusicPlayer::streamWorker(void* argument) {
    streamLoop(*static_cast<Voice*>(argument));
}

void MusicPlayer::streamLoop(Voice& voice) {
    while (voice.running.load(std::memory_order_acquire)) {
        bool filled = false;
        for (std::size_t index = 0; index < BufferCount; ++index) {
            if (voice.waveBuffers[index].status != NDSP_WBUF_DONE) {
                continue;
            }
            if (!fill(voice, index)) {
                voice.running.store(false, std::memory_order_release);
                break;
            }
            ndspChnWaveBufAdd(voice.channelId, &voice.waveBuffers[index]);
            filled = true;
        }
        svcSleepThread(filled ? 1000000LL : 4000000LL);
    }
}

bool MusicPlayer::fill(Voice& voice, std::size_t index) {
    auto* destination = static_cast<char*>(voice.audioData[index]);
    std::size_t written = 0;
    int bitstream = 0;
    int restartCount = 0;
    while (written < BufferSize) {
        const FsGuard guard;
        const long result = ov_read(&voice.vorbis, destination + written, BufferSize - written, &bitstream);
        if (result > 0) {
            written += static_cast<std::size_t>(result);
        } else if (result == 0 && restartCount++ == 0 && ov_raw_seek(&voice.vorbis, 0) == 0) {
            continue;
        } else {
            break;
        }
    }
    if (written == 0) {
        return false;
    }

    ndspWaveBuf& waveBuffer = voice.waveBuffers[index];
    std::memset(&waveBuffer, 0, sizeof(waveBuffer));
    waveBuffer.data_vaddr = voice.audioData[index];
    waveBuffer.nsamples = written / (static_cast<std::size_t>(voice.channels) * sizeof(std::int16_t));
    DSP_FlushDataCache(voice.audioData[index], written);
    return true;
}
