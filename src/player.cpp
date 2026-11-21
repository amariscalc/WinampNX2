#include "player.hpp"
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <cctype>
#include <set>
#include <stdexcept>
std::string upper(std::string s) {
    for(char& c:s) if(c>='a' && c<='z') c-=32;
    return s;
}
static std::string join(const std::string& path,const std::string& name) {
    return path+(path.back()=='/'?"":"/")+name;
}
std::vector<Entry> listMusic(const std::string& path) {
    static const std::set<std::string> types={"AAC","M4A","MP4","M4B","ALAC","FLAC","WAV","WAVE","AIF","AIFC","AIFF","MP3",
        "OGG","OGA","OPUS","WV","APE","MKA","WEBM","WMA","CAF"};
    std::unique_ptr<DIR,int(*)(DIR*)> dir(opendir(path.c_str()),closedir);
    if(!dir) throw std::runtime_error(std::string("Open folder: ")+std::strerror(errno));
    std::vector<Entry> entries;
    while(auto* item=readdir(dir.get())) {
        std::string name=item->d_name;
        if(name.empty() || name.front()=='.') continue;
        std::string full=join(path,name);
        struct stat info{};
        if(stat(full.c_str(),&info)!=0) continue;
        bool folder=S_ISDIR(info.st_mode);
        auto dot=name.rfind('.');
        std::string ext=dot==std::string::npos?"":upper(name.substr(dot+1));
        if(folder || (S_ISREG(info.st_mode) && types.count(ext)))
            entries.push_back({full,folder?name:name.substr(0,dot),ext,folder});
    }
    std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b) {
        if(a.directory!=b.directory) return a.directory;
        return upper(a.name)<upper(b.name);
    });
    return entries;
}
Player::Player(Platform& d,std::string r,std::string f):device(d),root(std::move(r)),folder(std::move(f)),rng(std::time(nullptr)) {
    try { loadFolder(folder); }
    catch(const std::exception&) {loadFolder(root);}
    for(auto& file:files) if(!file.directory) queue.push_back(file);
    if(queue.empty()) view=View::Files;
    eq.rebuild();
}
void Player::loadFolder(std::string path) {
    auto fresh=listMusic(path);
    if(path!=root) {
        auto pos=path.find_last_of('/');
        std::string parent=pos==std::string::npos?root:path.substr(0,pos);
        if(parent.size()<root.size()) parent=root;
        fresh.insert(fresh.begin(),{parent,"..","",true});
    }
    files=std::move(fresh); folder=path; fileRow=0; notice.clear();
}
void Player::stop() {
    device.clearAudio(); audio.reset(); decoded.clear(); offset=0;
    current=-1; position=submitted=0; paused=ended=false; bars.fill(0);
}
void Player::fail(const std::string& message) {stop(); notice=message;}
void Player::play(int index) {
    if(index<0 || index>=int(queue.size())) return;
    stop(); selected=index; notice.clear();
    try {
        auto next=std::make_unique<Decoder>();
        next->open(queue[index].path);
        device.pause(false);
        audio=std::move(next); current=index; eq.rebuild();
    } catch(const std::exception& e) {fail(e.what());}
}
void Player::toggle() {
    if(!audio) {play(selected);return;}
    device.pause(!paused); paused=!paused;
}
void Player::step(int delta,bool automatic) {
    int n=queue.size();
    if(!n) return;
    int next=current<0 ? selected : current+delta;
    if(shuffle && current>=0 && n>1) next=(current+std::uniform_int_distribution<int>(1,n-1)(rng))%n;
    if(automatic && next>=n && !repeat) {stop();return;}
    play((next%n+n)%n);
}
void Player::seek(double seconds) {
    if(!audio || audio->duration<=0) return;
    seconds=std::clamp(seconds,0.,std::max(0.,audio->duration-.02));
    if(!audio->seek(seconds)) {notice="Seeking unavailable";return;}
    device.clearAudio(); decoded.clear(); offset=0; ended=false;
    eq.rebuild(); position=submitted=seconds; bars.fill(0);
    if(!paused) device.pause(false);
}
void Player::tick() {
    if(!audio || paused) return;
    while(!ended && device.canQueue()) {
        std::array<int16_t,blockFrames*2> pcm{};
        int filled=0;
        while(filled<blockFrames) {
            if(offset>=decoded.size()) {
                if(!audio->read(decoded)) {ended=true;break;}
                offset=0;
            }
            int count=std::min<size_t>((decoded.size()-offset)/2,blockFrames-filled);
            eq.render(decoded.data()+offset,count,pcm.data()+2*filled);
            offset+=2*count; filled+=count;
        }
        if(filled) {
            device.submit(pcm.data(),filled); submitted+=double(filled)/Decoder::rate;
            auto newBars=spectrum(pcm.data(),filled);
            for(size_t i=0;i<bars.size();i++) bars[i]=std::max(newBars[i],bars[i]*.86f);
        }
    }
    int queued=device.queued(); position=std::max(0.,submitted-queued/double(Decoder::rate));
    if(!queued && ended) step(1,true);
}
void Player::chooseFile() {
    if(files.empty()) return;
    auto chosen=files[fileRow];
    if(chosen.directory) {loadFolder(chosen.path);return;}
    queue.clear();
    for(auto& f:files) if(!f.directory) queue.push_back(f);
    auto it=std::find_if(queue.begin(),queue.end(),[&](const Entry& e){return e.path==chosen.path;});
    view=View::Playlist; play(int(it-queue.begin()));
}
void Player::toggleEq(){eq.on=!eq.on;eq.rebuild();}
void Player::setGain(int value) {
    (band==0?eq.preamp:eq.gain[band-1])=std::clamp(value,-12,12);
    eq.on=true;eq.rebuild();
}
void Player::enter() {
    if(view==View::Files) chooseFile();
    else if(view==View::Eq) toggleEq();
    else if(selected==current) toggle();
    else play(selected);
}
void Player::input(Key key) {
    switch(key) {
        case Key::Browse: view=view==View::Files?View::Playlist:View::Files; break;
        case Key::Eq: view=view==View::Eq?View::Playlist:View::Eq; break;
        case Key::Back:
            if(view==View::Files && folder!=root && !files.empty()) loadFolder(files.front().path);
            else if(view!=View::Playlist) view=View::Playlist;
            else stop();
            break;
        case Key::Up: case Key::Down: {
            int delta=key==Key::Up?-1:1;
            if(view==View::Eq) setGain((band==0?eq.preamp:eq.gain[band-1])-delta);
            else if(view==View::Files) fileRow=std::clamp(fileRow+delta,0,std::max(0,int(files.size())-1));
            else selected=std::clamp(selected+delta,0,std::max(0,int(queue.size())-1));
            break;
        }
        case Key::Left: case Key::Right: {
            int dir=key==Key::Left?-1:1;
            if(view==View::Eq) band=std::clamp(band+dir,0,10);
            else seek(position+dir*5);
            break;
        }
        case Key::Accept: enter(); break;
        case Key::Shuffle: shuffle=!shuffle;break;
        case Key::Repeat: repeat=!repeat;break;
        case Key::Prev: case Key::Next: step(key==Key::Prev?-1:1);break;
        case Key::VolDown: case Key::VolUp: eq.volume=std::clamp(eq.volume+(key==Key::VolDown?-5:5),0,100); break;
        default: break;
    }
}
int Player::topRow() const {
    return std::max(0,(view==View::Files?fileRow:selected)-visible+1);
}
