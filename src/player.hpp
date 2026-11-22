#pragma once
#include "decoder.hpp"
#include "playlist.hpp"
#include "dsp.hpp"
#include "platform.hpp"
#include <memory>
#include <random>

enum class View {Playlist,Files,Eq};
struct Player {
    Platform& device;
    std::string root, folder, notice, queueName="LOCAL PLAYLIST", musicFolder;
    std::vector<Entry> queue,files,draft;
    bool menu=false, hasSession=false;
    int menuRow=0;
    enum MixAction {Add, Undo, SaveDraft, SaveQueue, OpenSaved, ActionCount};
    void mixAction(MixAction action);
    void openMenu();
    std::string menuLabel(MixAction action) const;
    std::unique_ptr<Decoder> audio;
    Equalizer eq;
    std::array<float,32> bars{};
    View view=View::Playlist;
    int current=-1, selected=0, fileRow=0, band=0;
    bool paused=false, shuffle=false, repeat=false;
    double position=0;
    explicit Player(Platform& device,std::string root,std::string folder);
    void play(int index,bool startPaused=false);
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
