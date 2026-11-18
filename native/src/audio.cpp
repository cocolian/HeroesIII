#include "h3/audio.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>

namespace h3 {
bool ClientSettings::load(const std::filesystem::path&path,std::string&error){
 try{if(!std::filesystem::exists(path))return true;if(std::filesystem::file_size(path)>4096)throw std::runtime_error("Audio options file too large");
  std::ifstream input(path);auto j=nlohmann::json::parse(input);if(j.at("version")!=1||!j.at("music").is_number_integer()||!j.at("effects").is_number_integer()||!j.at("muted").is_boolean())throw std::runtime_error("Invalid audio options");
  for(const auto&[name,limit]:std::array<std::pair<const char*,int>,4>{{{"music",9},{"effects",9},{"heroSpeed",3},{"scrollSpeed",2}}})if(j.contains(name)&&(!j[name].is_number_integer()||j[name]<0||j[name]>limit))throw std::runtime_error("Invalid client option value");
  ClientSettings candidate{j.at("music").get<int>(),j.at("effects").get<int>(),j.at("muted").get<bool>()};candidate.heroSpeed=j.value("heroSpeed",3);candidate.scrollSpeed=j.value("scrollSpeed",1);candidate.showPath=j.value("showPath",true);candidate.townOutlines=j.value("townOutlines",true);candidate.battleGrid=j.value("battleGrid",true);candidate.hdArtwork=j.value("hdArtwork",true);
  if(candidate.music<0||candidate.music>9||candidate.effects<0||candidate.effects>9||candidate.heroSpeed<0||candidate.heroSpeed>3||candidate.scrollSpeed<0||candidate.scrollSpeed>2)throw std::runtime_error("Invalid client options");*this=candidate;return true;
 }catch(const std::exception&e){error=e.what();return false;}
}
bool ClientSettings::save(const std::filesystem::path&path,std::string&error)const{
 try{if(music<0||music>9||effects<0||effects>9||heroSpeed<0||heroSpeed>3||scrollSpeed<0||scrollSpeed>2)throw std::runtime_error("Invalid client options");if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());auto temporary=path;temporary+=".tmp";std::ofstream output(temporary);output<<nlohmann::json{{"version",1},{"music",music},{"effects",effects},{"muted",muted},{"heroSpeed",heroSpeed},{"scrollSpeed",scrollSpeed},{"showPath",showPath},{"townOutlines",townOutlines},{"battleGrid",battleGrid},{"hdArtwork",hdArtwork}}.dump(2)<<'\n';output.close();if(!output)throw std::runtime_error("Audio options write failed");std::filesystem::rename(temporary,path);return true;
 }catch(const std::exception&e){error=e.what();return false;}
}
AudioEngine::AudioEngine(std::filesystem::path root,bool device):root_(std::move(root)){
 try{auto path=root_/"assets/audio/manifest.json";if(!std::filesystem::exists(path))return;if(std::filesystem::file_size(path)>2097152)throw std::runtime_error("Audio catalog too large");std::ifstream file(path);file>>catalog_;if(catalog_.value("schema",0)!=1)throw std::runtime_error("Invalid audio catalog");
 }catch(const std::exception&e){std::cerr<<"Audio catalog: "<<e.what()<<'\n';catalog_=nlohmann::json{};return;}
 if(device){if(SDL_InitSubSystem(SDL_INIT_AUDIO)!=0){std::cerr<<"Audio: "<<SDL_GetError()<<'\n';return;}
  SDL_AudioSpec desired{};desired.freq=AudioRate;desired.format=AUDIO_F32SYS;desired.channels=2;desired.samples=1024;desired.callback=callback;desired.userdata=this;
  device_=SDL_OpenAudioDevice(nullptr,0,&desired,nullptr,0);if(device_)SDL_PauseAudioDevice(device_,0);else std::cerr<<"Audio output: "<<SDL_GetError()<<'\n';
 }
}
AudioEngine::~AudioEngine(){if(device_){SDL_PauseAudioDevice(device_,1);SDL_CloseAudioDevice(device_);}}
void AudioEngine::callback(void*self,Uint8*data,int bytes){std::memset(data,0,size_t(bytes));auto&engine=*static_cast<AudioEngine*>(self);std::lock_guard lock(engine.mutex_);engine.mixer_.render({reinterpret_cast<float*>(data),size_t(bytes)/sizeof(float)});engine.outputFrames_.fetch_add(size_t(bytes)/sizeof(float)/2);}
AudioClip AudioEngine::load(const std::string&category,const std::string&name){
 std::string key=category+":"+name;if(auto found=cache_.find(key);found!=cache_.end()){found->second.used=++sequence_;return found->second.clip;}
 AudioClip result;bool music=category=="music";
 try{
  if((category!="music"&&category!="sounds")||!catalog_.contains(category)||!catalog_[category].contains(name))return {};
  std::filesystem::path relative=catalog_[category][name].at("file").get<std::string>();std::string generic=relative.generic_string();
  if(relative.is_absolute()||!generic.starts_with("assets/audio/")||std::any_of(relative.begin(),relative.end(),[](const auto&p){return p=="..";}))throw std::runtime_error("Unsafe audio path");
  auto path=root_/relative;auto size=std::filesystem::file_size(path);if(!size||size>100000000)throw std::runtime_error("Audio file size out of range");
  SDL_AudioSpec spec{};Uint8*data=nullptr;Uint32 bytes=0;
  if(!SDL_LoadWAV(path.string().c_str(),&spec,&data,&bytes))throw std::runtime_error(SDL_GetError());
  std::unique_ptr<Uint8,decltype(&SDL_FreeWAV)> memory(data,SDL_FreeWAV);
  if(spec.freq<8000||spec.freq>192000||spec.channels<1||spec.channels>2||bytes>100000000)throw std::runtime_error("Audio format out of range");
  // These are complete WAV clips. SDL2's streaming converter can retain its
  // last resampler block when one Put bypasses the staging buffer, so use
  // the whole-buffer conversion API to preserve the final music samples.
  SDL_AudioCVT converter{};
  if(SDL_BuildAudioCVT(&converter,spec.format,spec.channels,spec.freq,AUDIO_F32SYS,2,AudioRate)<0)throw std::runtime_error(SDL_GetError());
  if(converter.len_mult<=0||uint64_t(bytes)*converter.len_mult>400000000)throw std::runtime_error("Audio conversion buffer too large");
  std::unique_ptr<Uint8,decltype(&SDL_free)> converted(static_cast<Uint8*>(SDL_malloc(size_t(bytes)*converter.len_mult)),SDL_free);
  if(!converted)throw std::runtime_error("Audio conversion allocation failed");
  converter.buf=converted.get();converter.len=int(bytes);std::memcpy(converter.buf,data,bytes);
  if(SDL_ConvertAudio(&converter)!=0)throw std::runtime_error(SDL_GetError());
  int available=converter.len_cvt;if(available<=0||available>200000000||available%8)throw std::runtime_error("Converted audio size out of range");
  auto clip=std::make_shared<AudioBuffer>();clip->samples.resize(size_t(available)/sizeof(float));std::memcpy(clip->samples.data(),converter.buf,size_t(available));result=std::move(clip);
 }catch(const std::exception&e){std::cerr<<"Audio "<<name<<": "<<e.what()<<'\n';}
 cache_[key]={result,music,++sequence_};int limit=music?2:96;
 while(std::count_if(cache_.begin(),cache_.end(),[music](const auto&e){return e.second.music==music;})>limit){auto oldest=cache_.end();for(auto it=cache_.begin();it!=cache_.end();++it)if(it->second.music==music&&it->first!=key&&(oldest==cache_.end()||it->second.used<oldest->second.used))oldest=it;if(oldest==cache_.end())break;cache_.erase(oldest);}
 return result;
}
void AudioEngine::settings(const ClientSettings&settings){std::lock_guard lock(mutex_);mixer_.volumes(settings.music,settings.effects,settings.muted);}
void AudioEngine::music(const std::string&name,bool loop){if(theme_==name)return;theme_=name;auto clip=load("music",name);std::lock_guard lock(mutex_);mixer_.music(std::move(clip),loop);}
bool AudioEngine::effect(const std::string&name,float gain,float pan,uint64_t delay,uint64_t tag){if(name.empty())return false;auto clip=load("sounds",name);if(!clip)return false;std::lock_guard lock(mutex_);bool played=mixer_.play(std::move(clip),AudioBus::Effects,false,gain,pan,delay,tag);if(played){if(played_.size()>=256)played_.erase(played_.begin());played_.push_back(name);}return played;}
std::string AudioEngine::unitSound(int unit,const std::string&action)const{if(!catalog_.contains("units"))return {};auto at=catalog_["units"].find(std::to_string(unit));return at==catalog_["units"].end()?"":at->value(action,std::string{});}
void AudioEngine::consume(std::span<const GameEvent>events){
 uint64_t delay=0;
 for(const auto&e:events)switch(e.type){
 case GameEventType::HeroMove:effect("horse00.wav",.65f,0,0,1);break;
 case GameEventType::Visit:{const char*name=e.detail==int(ObjectType::Mine)?"flagmine.wav":e.detail==int(ObjectType::Treasure)?"chest.wav":e.detail==int(ObjectType::Artifact)?"treasure.wav":e.detail==int(ObjectType::Obelisk)?"obelisk.wav":e.detail==int(ObjectType::Resource)?"pickup01.wav":"";if(e.detail==int(ObjectType::Site))switch(e.unit){case 14:case 28:case 30:case 38:case 52:name="luck.wav";break;case 11:case 31:case 56:case 64:case 110:name="morale.wav";break;case 94:name="store.wav";break;case 96:name="temple.wav";break;case 48:name="faerie.wav";break;case 55:name="expernce.wav";break;}effect(name);break;}
 case GameEventType::NewDay:effect((e.detail-1)%28==0?"newmonth.wav":(e.detail-1)%7==0?"newweek.wav":"newday.wav");break;
 case GameEventType::Build:effect("buildtwn.wav");break;
 case GameEventType::Recruit:case GameEventType::Upgrade:effect("military.wav");break;
 case GameEventType::Level:effect("nwherolv.wav",1,0,0,2);break;
 case GameEventType::CombatStart:battleTrack_=battleNumber_%4+1;effect("battle0"+std::to_string(battleNumber_++%8)+".wav");break;
 case GameEventType::CombatMove:effect(unitSound(e.unit,"move"),1,0,e.scheduled?0:delay);if(!e.scheduled)delay+=AudioRate/5;break;
 case GameEventType::CombatAttack:effect(unitSound(e.unit,e.detail?"shoot":"attack"),1,0,e.scheduled?0:delay);if(!e.scheduled)delay+=AudioRate/6;break;
 case GameEventType::CombatHit:effect(unitSound(e.unit,"wince"),1,0,e.scheduled?0:delay);break;
 case GameEventType::CombatDeath:effect(unitSound(e.unit,"killed"),1,0,e.scheduled?0:delay);break;
 case GameEventType::SpellCast:{if(catalog_.contains("spells")){auto at=catalog_["spells"].find(std::to_string(e.detail));if(at!=catalog_["spells"].end())effect(at->get<std::string>(),1,0,e.scheduled?0:delay);}if(!e.scheduled)delay+=AudioRate/5;break;}
 case GameEventType::CombatLuck:effect("goodluck.wav",1,0,e.scheduled?0:delay);break;
 case GameEventType::CombatMorale:if(e.detail)effect(e.detail>0?"goodmrle.wav":"badmrle.wav",1,0,e.scheduled?0:delay);break;
 case GameEventType::CombatCreatureEffect:{const char*files[]={"disease.wav","bind.wav","paralyze.wav","paralyze.wav","poison.wav","age.wav","acid.wav","acid.wav","deathstr.wav","resurect.wav","resurect.wav","lightblt.wav","dispell.wav","fear.wav"};if(e.detail>=70&&e.detail<84)effect(files[e.detail-70]);break;}case GameEventType::SiegeHit:case GameEventType::CombatSpellEffect:case GameEventType::CombatEnd:break;
 }
}
void AudioEngine::ambients(std::span<const AmbientCue>cues){
 std::set<uint64_t>next;for(const auto&cue:cues){if(next.size()>=8)break;if(!cue.tag||!next.insert(cue.tag).second)continue;
  auto found=ambient_.find(cue.tag);if(found!=ambient_.end()&&found->second==cue.name){std::lock_guard lock(mutex_);mixer_.update(cue.tag,AudioBus::Ambient,cue.gain,cue.pan);continue;}
  auto clip=load("sounds",cue.name);std::lock_guard lock(mutex_);mixer_.stop(cue.tag,AudioBus::Ambient);if(clip){mixer_.play(std::move(clip),AudioBus::Ambient,true,cue.gain,cue.pan,0,cue.tag);ambient_[cue.tag]=cue.name;}
 }
 for(auto it=ambient_.begin();it!=ambient_.end();)if(!next.contains(it->first)){std::lock_guard lock(mutex_);mixer_.stop(it->first,AudioBus::Ambient);it=ambient_.erase(it);}else ++it;
}
void AudioEngine::offline(std::span<float>data){std::lock_guard lock(mutex_);mixer_.render(data);}
void AudioEngine::clear(){std::lock_guard lock(mutex_);mixer_.clear();theme_.clear();ambient_.clear();played_.clear();}
std::vector<std::string> AudioEngine::takePlayed(){std::lock_guard lock(mutex_);std::vector<std::string>result;result.swap(played_);return result;}
}
