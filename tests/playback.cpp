#include "player.hpp"
#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <thread>
#include <chrono>

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
    std::cout<<"PCM equality, pause, seek, EQ, folder navigation and end-of-track transitions passed.\n";
}
