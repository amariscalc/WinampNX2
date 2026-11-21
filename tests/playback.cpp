#include "player.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include "skin.hpp"
#include <map>
#include <set>
#include <thread>
#include <chrono>

static int rowFor(const Player& player,const std::string& name) {
    for(size_t i=0;i<player.files.size();i++) if(player.files[i].name==name) return int(i);
    throw std::runtime_error("Missing fixture: "+name);
}
static void checkMixes(Player& p,const std::string& root) {
    namespace fs=std::filesystem;
    auto album=fs::path(root)/"Albums";
    fs::copy_file(fs::path(root)/"flac.flac",album/"Música.flac");
    p.play(0);
    auto* playing=p.audio.get();
    p.input(Key::Browse);p.fileRow=rowFor(p,"wave");
    p.input(Key::Mix);p.input(Key::Accept);
    assert(p.draft.size()==1 && p.audio.get()==playing);
    p.fileRow=rowFor(p,"Albums");p.enter();
    p.fileRow=rowFor(p,"Música");
    Skin skin;
    skin.touch(p,{Key::Tap,603,69+20*(p.fileRow-p.topRow())+4});
    assert(p.draft.size()==2 && p.audio.get()==playing);
    p.mixAction(Player::Add);p.mixAction(Player::Undo);
    p.input(Key::Mix);p.input(Key::Down);p.input(Key::Down);p.input(Key::Accept);
    assert(p.draft.empty() && p.audio.get()==playing);
    auto dir=fs::path(root)/"Playlists";
    auto saved=dir/"Mix-001.m3u8";
    assert(fs::exists(saved));
    auto result=readMix(root,saved.string());
    assert(result.tracks.size()==2 && result.skipped==0);
    assert(result.tracks[0].name=="wave" && result.tracks[1].name=="Música");
    p.mixAction(Player::SaveQueue);
    assert(fs::exists(dir/"Mix-002.m3u8") && readMix(root,saved.string()).tracks.size()==2);
    p.mixAction(Player::OpenSaved);
    p.fileRow=rowFor(p,"Mix-001");p.enter();
    assert(p.queueName=="Mix-001" && p.current==0 && p.queue.size()==2 && p.audio);
    p.step(1);assert(p.current==1 && p.audio->format=="flac");
    auto imported=dir/"External.m3u";
    std::ofstream(imported,std::ios::binary)<<"\xef\xbb\xbf#EXTM3U\r\n#EXTINF:3,title\r\n"
        <<"..\\Albums\\Música.flac\r\n/wave.wav\nsdmc:/wave.wav\nhttps://example.org/other.mp3\n"
        <<"missing.flac\n../../outside.wav\nMix-001.m3u8\n";
    result=readMix(root,imported.string());
    assert(result.skipped==4 && result.tracks.size()==3 && result.tracks[0].name=="Música");
    assert(result.tracks[1].path==result.tracks[2].path);
    std::ofstream(dir/"Empty.m3u8")<<"#EXTM3U\nmissing.mp3\n";
    p.mixAction(Player::OpenSaved);p.fileRow=rowFor(p,"Empty");
    playing=p.audio.get();p.enter();
    assert(p.audio.get()==playing && p.queue.size()==2 && !p.notice.empty());
    p.fileRow=rowFor(p,"External");p.enter();
    assert(p.queue.size()==3 && p.audio && p.notice=="Skipped 4 unavailable");
    p.mixAction(Player::Add);
    fs::rename(dir,fs::path(root)/"moved");
    std::ofstream(dir)<<"folder name occupied";
    p.mixAction(Player::SaveDraft);
    assert(p.draft.size()==1 && p.audio && !p.notice.empty());
    std::cout<<"Mix creation, touch and controller actions, import, playback and save failures passed.\n";
}

// Run with the fixtures created by check-audio.sh.
int main(int argc,char** argv) {
    assert(argc==2);
    std::string folder=argv[1];
    std::map<std::string,std::vector<int16_t>> output;
    auto entries=listMusic(folder);
    Equalizer eq;
    for(auto& file:entries) {
        if(file.directory) continue;
        Decoder d;d.open(file.path);
        auto& pcm=output[file.name];
        std::vector<float> chunk;
        while(d.read(chunk)) {
            size_t start=pcm.size();pcm.resize(start+chunk.size());
            eq.render(chunk.data(),chunk.size()/2,pcm.data()+start);
        }
        assert(pcm.size()>48000*2 && pcm.size()<48000*5*2);
        assert(std::any_of(pcm.begin(),pcm.end(),[](int v){return v!=0;}));
        assert(d.seek(1.0));assert(d.read(chunk) && !chunk.empty());
        std::cout<<file.name<<" "<<d.format<<" "<<d.sourceRate<<" Hz, "<<pcm.size()/2<<" stereo frames\n";
    }
    for(auto name:{"flac","alac","wavpack","aiff"}) assert(output.at(name)==output.at("wave"));
    assert(output.size()==11);
    Platform device;device.open();
    Player p(device,folder,folder);
    p.play(0);assert(p.audio);p.tick();
    p.toggle();assert(p.paused);p.seek(.7);p.toggle();p.tick();assert(!p.paused);
    p.step(1);assert(p.current==1);p.tick();
    p.input(Key::Eq);p.input(Key::Right);p.input(Key::Up);assert(p.eq.on && p.eq.gain[0]==1);
    p.input(Key::Browse);assert(p.view==View::Files);
    p.fileRow=0;p.chooseFile();assert(p.folder!=folder);p.input(Key::Back);assert(p.folder==folder);
    p.input(Key::Browse);assert(p.view==View::Playlist);
    p.play(p.queue.size()-1);p.seek(p.audio->duration-.4);
    for(int i=0;i<200 && p.audio;i++) {p.tick();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    assert(!p.audio);
    p.play(0);p.seek(p.audio->duration-.4);
    for(int i=0;i<200 && p.current==0;i++) {p.tick();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    assert(p.current==1);
    p.stop();
    checkMixes(p,folder);
    std::cout<<"PCM equality, pause, seek, EQ, folder navigation and end-of-track transitions passed.\n";
}
