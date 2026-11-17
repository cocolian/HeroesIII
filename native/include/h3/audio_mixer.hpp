#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace h3 {
constexpr int AudioRate=48000;
struct AudioBuffer {std::vector<float> samples;size_t frames()const{return samples.size()/2;}};
using AudioClip=std::shared_ptr<const AudioBuffer>;
enum class AudioBus {Music,Effects,Ambient};
struct AudioVoice {AudioClip clip;size_t cursor=0;bool loop=false;float gain=1,pan=0;uint64_t delay=0,tag=0,order=0;AudioBus bus=AudioBus::Effects;};
class AudioMixer {
public:
 void volumes(int music,int effects,bool muted=false);
 void music(AudioClip,bool loop=true,uint64_t fadeFrames=AudioRate/4);
 bool play(AudioClip,AudioBus=AudioBus::Effects,bool loop=false,float gain=1,float pan=0,uint64_t delay=0,uint64_t tag=0);
 void stop(uint64_t tag,AudioBus);void clear();void render(std::span<float> stereo);
 bool update(uint64_t tag,AudioBus,float gain,float pan);
 size_t activeVoices()const;size_t musicCursor()const{return music_?music_->cursor:0;}
private:
 std::optional<AudioVoice> music_,previous_;std::array<std::optional<AudioVoice>,32> voices_{};
 uint64_t fade_=0,fadeLength_=0,sequence_=0;float musicGain_=.5f,effectsGain_=.5f;bool muted_=false;
 static std::array<float,2> sample(AudioVoice&);
};
}
