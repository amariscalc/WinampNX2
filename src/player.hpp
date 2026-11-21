#pragma once
#include "decoder.hpp"
#include "dsp.hpp"
#include "platform.hpp"
#include <memory>
#include <random>

struct Entry {std::string path,name,ext; bool directory;};
std::vector<Entry> listMusic(const std::string& path);
std::string upper(std::string text);
enum class View {Playlist,Files,Eq};
struct Player {
    Platform& device;
    std::string root, folder, notice;
    std::vector<Entry> queue,files;
    std::unique_ptr<Decoder> audio;
    Equalizer eq;
    std::array<float,32> bars{};
    View view=View::Playlist;
    int current=-1, selected=0, fileRow=0, band=0;
    bool paused=false, shuffle=false, repeat=false;
    double position=0;
    explicit Player(Platform& device,std::string root,std::string folder);
    void play(int index);
    void toggle();
    void stop();
    void seek(double seconds);
    void step(int delta,bool automatic=false);
    void tick();
    void input(Key key);
    void enter();
    void loadFolder(std::string path);
    void chooseFile();
    void setGain(int value);
    void toggleEq();
    void fail(const std::string& message);
    int topRow() const;
    static constexpr int visible=9;
private:
    std::vector<float> decoded;
    size_t offset=0;
    bool ended=false;
    double submitted=0;
    std::mt19937 rng;
};
