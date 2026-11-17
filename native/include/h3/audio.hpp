#pragma once
#include "h3/audio_mixer.hpp"
#include "h3/game.hpp"
#include <SDL.h>
#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

namespace h3 {
struct ClientSettings {
 int music=5,effects=5;bool muted=false;
 int heroSpeed=3,scrollSpeed=1;bool showPath=true,townOutlines=true,battleGrid=true,hdArtwork=true;
 bool load(const std::filesystem::path&,std::string&);bool save(const std::filesystem::path&,std::string&)const;
 bool operator==(const ClientSettings&)const=default;
};
struct AmbientCue {std::string name;uint64_t tag;float gain=1,pan=0;};
class AudioEngine {
public:
 explicit AudioEngine(std::filesystem::path root,bool device=true);~AudioEngine();
 AudioEngine(const AudioEngine&)=delete;AudioEngine&operator=(const AudioEngine&)=delete;
 AudioClip load(const std::string& category,const std::string& name);
 void music(const std::string&,bool loop=true);bool effect(const std::string&,float gain=1,float pan=0,uint64_t delay=0,uint64_t tag=0);
 void consume(std::span<const GameEvent>);void ambients(std::span<const AmbientCue>);void settings(const ClientSettings&);
 void offline(std::span<float>);void clear();bool available()const{return !catalog_.empty();}bool outputActive()const{return device_!=0;}
 void pause(bool paused){if(device_)SDL_PauseAudioDevice(device_,paused?1:0);}
 std::string theme()const{return theme_;}std::string battleTheme()const{return "combat."+std::to_string(battleTrack_);}
 std::vector<std::string> takePlayed();size_t cacheSize()const{return cache_.size();}
 uint64_t outputFrames()const{return outputFrames_.load();}
private:
 struct CacheEntry {AudioClip clip;bool music=false;uint64_t used=0;};
 std::filesystem::path root_;nlohmann::json catalog_;SDL_AudioDeviceID device_=0;AudioMixer mixer_;std::mutex mutex_;
 std::map<std::string,CacheEntry> cache_;std::map<uint64_t,std::string> ambient_;std::string theme_;std::vector<std::string> played_;uint64_t sequence_=0;int battleTrack_=1,battleNumber_=0;
 std::atomic<uint64_t> outputFrames_{0};
 static void callback(void*,Uint8*,int);std::string unitSound(int,const std::string&)const;
};
}
