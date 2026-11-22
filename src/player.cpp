#include "player.hpp"
#include <ctime>
#include <filesystem>
#include <cstring>
#include <cstdio>
#include <stdexcept>
Player::Player(Platform& d,std::string r,std::string f):device(d),root(std::move(r)),folder(std::move(f)),musicFolder(folder),rng(std::time(nullptr)) {
    try { loadFolder(folder); }
    catch(const std::exception&) {loadFolder(root);}
    for(auto& file:files) if(file.track()) queue.push_back(file);
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
void Player::play(int index,bool startPaused) {
    if(index<0 || index>=int(queue.size())) return;
    stop(); selected=index; notice.clear();
    try {
        auto next=std::make_unique<Decoder>();
        next->open(queue[index].path);
        paused=startPaused;
        if(!paused) device.pause(false);
        hasSession=true;
        audio=std::move(next); current=index; eq.rebuild();
    } catch(const std::exception& e) {fail(e.what());}
}
void Player::toggle() {
    if(!audio) {play(selected);return;}
    device.pause(!paused); paused=!paused;notice.clear();
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
    if(chosen.playlist()) {
        try {
            auto loaded=readMix(root,chosen.path);
            if(loaded.tracks.empty()) {notice="No playable tracks in this list";return;}
            queue=std::move(loaded.tracks);
            mixPath=loaded.skipped?std::string():chosen.path;queueName=chosen.name; view=View::Playlist;
            play(0);
            if(audio && loaded.skipped) notice="Skipped "+std::to_string(loaded.skipped)+" unavailable";
        } catch(const std::exception& error) {notice=error.what();}
        return;
    }
    mixPath.clear();queue.clear();queueName="LOCAL PLAYLIST";
    for(auto& f:files) if(f.track()) queue.push_back(f);
    auto it=std::find_if(queue.begin(),queue.end(),[&](const Entry& e){return e.path==chosen.path;});
    view=View::Playlist; play(int(it-queue.begin()));
}
void Player::nameKey(Key key) {
    const int count=std::strlen(alphabet);
    if(key==Key::Back) {naming=false;return;}
    if(key==Key::Up || key==Key::Down) letter=(letter+count+(key==Key::Up?1:-1))%count;
    if(key==Key::Left && !editName.empty()) editName.pop_back();
    if(key==Key::Right && editName.size()<64) editName+=alphabet[letter];
    if(key!=Key::Accept) return;
    if(editName.empty() || editName.back()==' ') {notice="Choose a name";return;}
    namespace fs=std::filesystem;
    try {
        auto target=fs::path(mixPath).parent_path()/(editName+".m3u8");
        if(target!=fs::path(mixPath)) {
            if(fs::exists(target)) throw std::runtime_error("Name already exists");
            fs::rename(mixPath,target);
        }
        mixPath=target.string();queueName=editName;naming=false;notice="Mix renamed";
    } catch(const std::exception&) {notice="Cannot rename: check name";}
}
void Player::editMix(int action) {
    namespace fs=std::filesystem;
    if(view!=View::Playlist || mixPath.empty() ||
       fs::path(mixPath).parent_path()!=fs::path(musicFolder)/"Playlists" ||
       upper(fs::path(mixPath).extension().string())!=".M3U8") {
        notice="Open a saved mix first";return;
    }
    auto disk=readMix(root,mixPath);
    if(disk.skipped || disk.tracks.size()!=queue.size() ||
       !std::equal(queue.begin(),queue.end(),disk.tracks.begin(),
           [](const Entry& a,const Entry& b){return a.path==b.path;})) {
        notice="Mix changed: reopen it";return;
    }
    if(action==Rename) {editName="";letter=0;naming=true;notice.clear();return;}
    auto next=queue;
    int dest=selected;
    int playing=current;
    if(action==RemoveTrack) {
        if(next.size()==1) {notice="Keep at least one track";return;}
        next.erase(next.begin()+selected);
        if(playing==selected) playing=-1;
        else if(playing>selected) --playing;
        dest=std::min(selected,int(next.size())-1);
    } else {
        dest+=action==MoveUp?-1:1;
        if(dest<0 || dest>=int(queue.size())) {notice="Already at end of list";return;}
        std::swap(next[selected],next[dest]);
        if(playing==selected) playing=dest;
        else if(playing==dest) playing=selected;
    }
    auto folder=fs::path(mixPath).parent_path();
    auto backup=mixPath+".bak";
    if(fs::exists(backup)) throw std::runtime_error("Recover the .bak file first");
    auto staged=folder/writeMix(musicFolder,next);
    if(std::rename(mixPath.c_str(),backup.c_str())!=0) {
        std::remove(staged.c_str());throw std::runtime_error("Cannot update mix");
    }
    if(std::rename(staged.c_str(),mixPath.c_str())!=0) {
        std::rename(backup.c_str(),mixPath.c_str());std::remove(staged.c_str());
        throw std::runtime_error("Cannot publish mix edit");
    }
    std::remove(backup.c_str());
    if(current>=0 && playing<0) stop();
    queue=std::move(next);selected=dest;current=playing;notice="Mix updated";
}
void Player::openMenu() {
    menu=!menu;menuRow=0;
}
std::string Player::menuLabel(MixAction action) const {
    switch(action) {
        case Add:return "Add selected track";
        case Undo:return "Remove last addition";
        case SaveDraft:return "Save mix ("+std::to_string(draft.size())+")";
        case SaveQueue:return "Save current playlist";
        case Rename:return "Rename mix";
        case MoveUp:return "Move track up";
        case MoveDown:return "Move track down";
        case RemoveTrack:return "Remove from mix";
        case OpenSaved:return "Open saved mixes";
        default:return {};
    }
}
void Player::mixAction(MixAction action) {
    menu=false;
    try {
        switch(action) {
            case Add: {
                const auto& items=view==View::Files?files:queue;
                int index=view==View::Files?fileRow:selected;
                if(index<0 || index>=int(items.size()) || !items[index].track()) {
                    notice="Select a track to add";break;
                }
                draft.push_back(items[index]);
                notice="Added to mix ("+std::to_string(draft.size())+")";break;
            }
            case Undo:
                if(draft.empty()) notice="Mix is empty";
                else {draft.pop_back();notice="Mix tracks: "+std::to_string(draft.size());}
                break;
            case SaveDraft: case SaveQueue: {
                auto name=writeMix(musicFolder,action==SaveDraft?draft:queue);
                if(action==SaveDraft) draft.clear();
                if(view==View::Files && folder==mixDirectory(musicFolder)) loadFolder(folder);
                notice="Saved "+name;break;
            }
            case Rename: case MoveUp: case MoveDown: case RemoveTrack: editMix(action);break;
            case OpenSaved:
                loadFolder(mixDirectory(musicFolder));view=View::Files;break;
            default:break;
        }
    } catch(const std::exception& error) {notice=error.what();}
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
    if(naming) {nameKey(key);return;}
    if(key==Key::Mix) {openMenu();return;}
    if(menu) {
        if(key==Key::Back) menu=false;
        else if(key==Key::Up) menuRow=std::max(0,menuRow-1);
        else if(key==Key::Down) menuRow=std::min(int(ActionCount)-1,menuRow+1);
        else if(key==Key::Accept) mixAction(static_cast<MixAction>(menuRow));
        return;
    }
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
