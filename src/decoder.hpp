#pragma once
#include <string>
#include <vector>
#include <cstdint>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}

class Decoder {
    AVFormatContext* file = nullptr;
    AVCodecContext* codec = nullptr;
    SwrContext* converter = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    int stream = -1;
    bool draining = false;
    double discardBefore = -1;
    int convert(const uint8_t** input, int count, std::vector<float>& samples);
    void close();
public:
    static constexpr int rate = 48000;
    int sourceRate = 0, bits = 0, channels = 0, kbps = 0;
    double duration = 0;
    std::string format, title, artist, album, genre;
    static constexpr int coverSide=128;
    std::vector<uint32_t> cover;
    int discNumber=0, trackNumber=0;
    double trackGain=1., albumGain=1.;
    void loadCover();
    Decoder() = default;
    Decoder(const Decoder&) = delete;
    ~Decoder() {close();}
    void open(const std::string& path);
    bool read(std::vector<float>& stereo);
    bool seek(double seconds);
};
