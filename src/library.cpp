#include "player.hpp"
#include <cstring>
#include <stdexcept>
#include <sys/stat.h>

namespace {
constexpr int maxDepth=32;

void findTracks(const std::string& folder,const std::string& query,
                std::vector<Entry>& results,int depth) {
    if(depth>maxDepth) return;
    for(auto entry:listMusic(folder)) {
        if(entry.directory) {
            struct stat info{};
            if(lstat(entry.path.c_str(),&info)!=0 || !S_ISDIR(info.st_mode)) continue;
            try { findTracks(entry.path,query,results,depth+1); }
            catch(const std::runtime_error&) { /* Removable folders can disappear during a scan. */ }
        } else if(entry.track()) {
            bool matches=query.empty() || upper(entry.name).find(query)!=std::string::npos;
            if(!matches) {
                try {
                    Decoder track;
                    track.open(entry.path);
                    matches=upper(track.title+" "+track.artist+" "+track.album).find(query)!=std::string::npos;
                } catch(const std::runtime_error&) { continue; }
            }
            if(matches) results.push_back(std::move(entry));
        }
    }
}
}

void Player::searchLibrary() {
    std::vector<Entry> results;
    try { findTracks(musicFolder,upper(searchQuery),results,0); }
    catch(const std::runtime_error& error) { notice=error.what();return; }
    libraryResults=std::move(results);
    libraryView=true;favoritesView=false;view=View::Files;fileRow=0;
    notice=std::to_string(libraryResults.size())+" matches";
}
void Player::searchKey(Key key) {
    const int count=std::strlen(alphabet);
    if(key==Key::Back || key==Key::Mix) {overlay=Overlay::None;return;}
    if(key==Key::Up || key==Key::Down)
        searchCharacter=(searchCharacter+count+(key==Key::Up?1:-1))%count;
    if(key==Key::Left && !searchQuery.empty()) searchQuery.pop_back();
    if(key==Key::Right && searchQuery.size()<64) searchQuery+=alphabet[searchCharacter];
    if(key==Key::Accept) {overlay=Overlay::None;searchLibrary();}
}
