#pragma once
#include <string>
#include <vector>

struct Entry {
    std::string path, name, ext;
    bool directory;
    bool playlist() const { return !directory && (ext=="M3U" || ext=="M3U8"); }
    bool track() const { return !directory && !playlist(); }
};
std::string upper(std::string text);
std::vector<Entry> listMusic(const std::string& path);
struct LoadedMix {std::vector<Entry> tracks; size_t skipped=0;};
LoadedMix readMix(const std::string& root,const std::string& file);
std::string mixDirectory(const std::string& musicFolder);
std::string writeMix(const std::string& musicFolder,const std::vector<Entry>& tracks);
