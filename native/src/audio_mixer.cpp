#include "h3/audio_mixer.hpp"
#include <algorithm>
#include <cmath>

namespace h3 {
void AudioMixer::volumes(int music,int effects,bool muted){musicGain_=std::clamp(music,0,9)/9.f;effectsGain_=std::clamp(effects,0,9)/9.f;muted_=muted;}
void AudioMixer::music(AudioClip clip,bool loop,uint64_t fadeFrames){
 if(clip&&(!clip->frames()||clip->samples.size()%2))clip.reset();
 if(music_&&music_->clip==clip&&music_->loop==loop)return;
 previous_=std::move(music_);music_.reset();if(clip)music_=AudioVoice{std::move(clip),0,loop};
 fade_=fadeLength_=fadeFrames;if(!fadeFrames)previous_.reset();
}
bool AudioMixer::play(AudioClip clip,AudioBus bus,bool loop,float gain,float pan,uint64_t delay,uint64_t tag){
 if(!clip||!clip->frames()||clip->samples.size()%2||bus==AudioBus::Music||!std::isfinite(gain)||!std::isfinite(pan))return false;
 int begin=bus==AudioBus::Ambient?24:0,end=bus==AudioBus::Ambient?32:24;int slot=-1;
 for(int i=begin;i<end;i++)if((tag&&voices_[i]&&voices_[i]->tag==tag)||!voices_[i]){slot=i;if(tag&&voices_[i])break;}
 if(tag)for(int i=begin;i<end;i++)if(voices_[i]&&voices_[i]->tag==tag){slot=i;break;}
 if(slot<0){slot=begin;for(int i=begin+1;i<end;i++)if(voices_[i]->order<voices_[slot]->order)slot=i;}
 voices_[slot]=AudioVoice{std::move(clip),0,loop,std::clamp(gain,0.f,1.f),std::clamp(pan,-1.f,1.f),delay,tag,++sequence_,bus};return true;
}
void AudioMixer::stop(uint64_t tag,AudioBus bus){for(auto&voice:voices_)if(voice&&voice->bus==bus&&voice->tag==tag)voice.reset();}
bool AudioMixer::update(uint64_t tag,AudioBus bus,float gain,float pan){if(!std::isfinite(gain)||!std::isfinite(pan))return false;for(auto&v:voices_)if(v&&v->tag==tag&&v->bus==bus){v->gain=std::clamp(gain,0.f,1.f);v->pan=std::clamp(pan,-1.f,1.f);return true;}return false;}
void AudioMixer::clear(){music_.reset();previous_.reset();for(auto&voice:voices_)voice.reset();fade_=fadeLength_=0;}
size_t AudioMixer::activeVoices()const{return std::count_if(voices_.begin(),voices_.end(),[](const auto&v){return v.has_value();});}
std::array<float,2> AudioMixer::sample(AudioVoice&voice){
 if(voice.delay){voice.delay--;return {};}
 if(voice.cursor>=voice.clip->frames()){if(voice.loop)voice.cursor=0;else return {};}
 size_t at=voice.cursor++*2;float left=voice.clip->samples[at],right=voice.clip->samples[at+1];
 if(!std::isfinite(left))left=0;if(!std::isfinite(right))right=0;
 return {left*voice.gain*(voice.pan>0?1-voice.pan:1),right*voice.gain*(voice.pan<0?1+voice.pan:1)};
}
void AudioMixer::render(std::span<float> stereo){
 std::fill(stereo.begin(),stereo.end(),0.f);
 for(size_t frame=0;frame<stereo.size()/2;frame++){
  float left=0,right=0;auto mix=[&](AudioVoice&voice,float gain){auto s=sample(voice);left+=s[0]*gain;right+=s[1]*gain;};
  float progress=fadeLength_?1-float(fade_)/float(fadeLength_):1;
  if(previous_)mix(*previous_,musicGain_*(1-progress));if(music_)mix(*music_,musicGain_*progress);
  for(auto&voice:voices_)if(voice){mix(*voice,effectsGain_);if(!voice->loop&&!voice->delay&&voice->cursor>=voice->clip->frames())voice.reset();}
  if(fade_&&!--fade_)previous_.reset();
  if(muted_)left=right=0;stereo[frame*2]=std::clamp(left,-1.f,1.f);stereo[frame*2+1]=std::clamp(right,-1.f,1.f);
 }
}
}
