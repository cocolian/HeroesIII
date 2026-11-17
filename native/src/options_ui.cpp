#include "h3/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace h3 {
namespace {constexpr int X=160,Y=56;constexpr SDL_Color GoldColor{236,213,130,255},Muted{139,126,101,255};}
void Desktop::applyOptions(){if(audio_)audio_->settings(settings_);std::string error;if(!settings_.save(saveDir_/"client-options.json",error))say(error);}
void Desktop::openOptions(){if(scene==Scene::Options)return;optionsReturn_=scene;scene=Scene::Options;focus_=-1;marching_=false;optionsDragging_=-1;armyDrag_.reset();mouseLeftDown_=false;pressedButton_=-1;}
SDL_Rect Desktop::volumeTrack(int kind)const{return {X+28,Y+(kind?425:359),190,36};}
void Desktop::optionVolume(int kind,int x){int level=std::clamp((x-volumeTrack(kind).x)/19,0,9);int&volume=kind?settings_.effects:settings_.music;if(volume==level)return;volume=level;applyOptions();}
std::string Desktop::audioTheme()const{
 Scene context=scene==Scene::Options?optionsReturn_:scene;
 for(int depth=0;depth<4;depth++){if(context==Scene::Market)context=marketReturn_;else if(context==Scene::Army)context=armyReturn_;else if(context==Scene::Split)context=splitReturn_;else break;}
 if(context==Scene::Menu||context==Scene::Scenario||context==Scene::Help||context==Scene::Codex)return "menu.main";
 if(game.battle||battleMotion_.active())return audio_?audio_->battleTheme():"combat.1";
 if(context==Scene::Outcome)return game.result>0?"report.scenario-win":"report.ultimate-loss";
 if(game.report)return game.result>0?"report.scenario-win":game.result<0?"report.ultimate-loss":game.report->won?"report.win":"report.loss";
 if(context==Scene::Town||context==Scene::Build||context==Scene::BuildConfirm||context==Scene::Fort||context==Scene::Recruit||context==Scene::MageGuild||context==Scene::University)return "town."+Factions[game.town.faction].key;
 const auto*tile=game.tile(game.hero.pos);return std::string("terrain.")+(tile&&tile->terrain==Terrain::Dirt?"dirt":tile&&tile->terrain==Terrain::Sand?"sand":tile&&tile->terrain==Terrain::Water?"water":"grass");
}
void Desktop::updateAudio(){
 auto events=game.takeEvents();auto motionEvents=battleMotion_.takeEvents();if(!audio_)return;audio_->consume(events);audio_->consume(motionEvents);auto theme=audioTheme();audio_->music(theme,!theme.starts_with("report."));
 std::vector<std::pair<float,AmbientCue>> nearby;
 Scene context=scene==Scene::Options?optionsReturn_:scene;if(context==Scene::Market)context=marketReturn_;
 if(context==Scene::Adventure&&!game.battle&&!game.report)for(const auto&object:game.objects){
  const auto*tile=game.tile(object.pos);if(!tile||!tile->seen)continue;float dx=object.pos.x+.5f-camX_,dy=object.pos.y+.5f-camY_,distance=std::hypot(dx,dy);if(distance>=6)continue;
  std::string name;if(object.type==ObjectType::Mine){const char*files[]={"loopmine.wav","looplumb.wav","loopmine.wav","loopmerc.wav","loopsulf.wav","loopcrys.wav","loopgemp.wav"};name=files[object.resource];}
  else if(object.type==ObjectType::TradingPost)name="loopmark.wav";else if(object.type==ObjectType::Well)name="loopfoun.wav";
  if(!name.empty())nearby.push_back({distance,{name,uint64_t(object.id),.28f*(1-distance/6),std::clamp(dx/6,-1.f,1.f)}});
 }
 std::stable_sort(nearby.begin(),nearby.end(),[](const auto&a,const auto&b){return a.first<b.first;});std::vector<AmbientCue> cues;for(const auto&entry:nearby){if(cues.size()==8)break;cues.push_back(entry.second);}audio_->ambients(cues);
}
void Desktop::options(){
 int oldX=mouseX_,oldY=mouseY_;mouseX_=mouseY_=-1000;
 switch(optionsReturn_){case Scene::Menu:menu();break;case Scene::Town:town();break;case Scene::Battle:battle();break;default:adventure();break;}
 mouseX_=oldX;mouseY_=oldY;buttons_.clear();originalImage("sysopbck.pcx",{X,Y,480,487});
 centerText(settings_.muted?"系统选项 · 静音":"系统选项",{X+25,Y+17,434,30},19,GoldColor);
 const char*labels[]={"英雄移动速度","敌方移动速度","地图滚动速度","画面质量（F7）","音乐音量","音效音量"};int ys[]={55,121,187,253,339,405};
 for(int i=0;i<6;i++)centerText(labels[i],{X+27,Y+ys[i],191,19},12,i==1?Muted:GoldColor);
 // Default Return closes this modal; the game remains in memory.
 buttons_.push_back({{X+357,Y+415,99,47},[this]{scene=optionsReturn_;focus_=-1;optionsDragging_=-1;},true,{},"确认选项并返回游戏（Enter / Esc）"});
 for(int i=0;i<4;i++){
  SDL_Rect r{X+27+i*48,Y+78,46,32};frame("sysopb"+std::to_string(i+1)+".def",0,settings_.heroSpeed==i?2:0,r);
  buttons_.push_back({r,[this,i]{settings_.heroSpeed=i;applyOptions();},true,{},"英雄移动动画速度："+std::to_string(i+1)+" / 4；不改变行动力消耗"});
  frame("sysopb"+std::to_string(i+5)+".def",0,i==2?2:0,{X+27+i*48,Y+144,46,32});box({X+27+i*48,Y+144,46,32},{0,0,0,100});buttons_.push_back({{X+27+i*48,Y+144,46,32},[]{},false,{},"敌方英雄动画速度：当前版本尚未实现"});
 }
 for(int i=0;i<3;i++){SDL_Rect r{X+27+i*64,Y+210,62,32};frame(i==0?"sysopb9.def":i==1?"sysob10.def":"sysob11.def",0,settings_.scrollSpeed==i?2:0,r);buttons_.push_back({r,[this,i]{settings_.scrollSpeed=i;applyOptions();},true,{},"地图边缘滚动速度："+std::to_string(i+1)+" / 3"});}
 button({X+27,Y+276,94,32},settings_.hdArtwork?"重绘 · 已选":"高清重绘",[this]{settings_.hdArtwork=true;applyOptions();});button({X+124,Y+276,94,32},settings_.hdArtwork?"经典像素":"经典 · 已选",[this]{settings_.hdArtwork=false;applyOptions();});
 const char*checks[]={"显示移动路径","移动英雄提醒","快速战斗","视频字幕","城镇建筑描边","魔法书动画"};
 for(int i=0;i<6;i++){bool active=i==0||i==4;bool checked=i==0?settings_.showPath:i==4?settings_.townOutlines:i==3||i==5;SDL_Rect r{X+246,Y+55+i*32,32,24};frame("sysopchk.def",0,checked?1:0,r);if(!active)box(r,{0,0,0,100});text(checks[i],X+287,Y+61+i*32,12,active?GoldColor:Muted);
  buttons_.push_back({{r.x,r.y,204,24},[this,i]{if(i==0)settings_.showPath=!settings_.showPath;else settings_.townOutlines=!settings_.townOutlines;applyOptions();},active,{},std::string(checks[i])+(active?"：点击切换开关":"：当前版本尚未实现")});
 }
 for(int kind=0;kind<2;kind++){auto track=volumeTrack(kind);int selected=kind?settings_.effects:settings_.music;frame("syslb.def",0,selected,{track.x+selected*19,track.y,18,36});
  for(int level=0;level<10;level++)buttons_.push_back({{track.x+level*19,track.y,19,36},[this,kind,level]{(kind?settings_.effects:settings_.music)=level;applyOptions();if(kind&&audio_)audio_->effect("button.wav");},true,{},std::string(kind?"音效音量：":"音乐音量：")+std::to_string(level)+" / 9；拖动滑块调节，F10 静音"});
 }
 buttons_.push_back({{X+246,Y+298,99,48},[this]{load();},true,{},"读取已保存的游戏（L）"});
 buttons_.push_back({{X+357,Y+298,99,48},[this]{std::string error;say(game.save(saveDir_/"manual.json",error)?"C++ 原生存档已保存。":error);},true,{},"保存当前游戏（S）"});
 box({X+246,Y+357,99,47},{0,0,0,115});buttons_.push_back({{X+246,Y+357,99,47},[]{},false,{},"重新开始场景：当前版本尚未实现"});
 buttons_.push_back({{X+357,Y+357,99,47},[this]{scene=Scene::Menu;focus_=-1;},true,{},"返回主菜单；可用 Esc 继续当前游戏"});
 buttons_.push_back({{X+246,Y+415,99,47},[this]{running_=false;},true,{},"退出游戏（Q）"});
}
void Desktop::optionsRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/(std::string(name)+".png")),"Options screenshot failed");};
 auto mouse=[this](Uint32 type,int x,int y){SDL_Event e{};e.type=type;if(type==SDL_MOUSEMOTION){e.motion.x=x;e.motion.y=y;}else{e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;}handleEvent(e);};
 settings_=ClientSettings{};game=Game::create();scene=Scene::Adventure;marching_=false;route_.clear();camX_=4.5f;camY_=20.5f;auto before=game.toJson();std::string originalTheme=audioTheme();key(SDLK_o,KMOD_NONE);
 check(scene==Scene::Options&&optionsReturn_==Scene::Adventure&&audioTheme()==originalTheme,"Options did not preserve parent music context");shot("options-default");
 click({X+29,Y+370},false);check(settings_.music==0&&game.toJson()==before,"Music volume changed game state");render();auto track=volumeTrack(1);
 mouse(SDL_MOUSEBUTTONDOWN,track.x+5,track.y+10);check(optionsDragging_==1,"Effects volume did not capture mouse");mouse(SDL_MOUSEMOTION,track.x+1000,track.y+10);mouse(SDL_MOUSEBUTTONUP,track.x+1000,track.y+10);
 check(settings_.effects==9&&optionsDragging_<0&&scene==Scene::Options&&game.toJson()==before,"Effects volume drag triggered another control");
 mouse(SDL_MOUSEBUTTONDOWN,track.x+10,track.y+10);SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;handleEvent(lost);check(optionsDragging_<0&&!mouseLeftDown_&&pressedButton_<0,"Focus loss retained volume capture");
 key(SDLK_END,KMOD_SHIFT);key(SDLK_F10,KMOD_NONE);check(settings_.muted&&settings_.effects==9,"Volume keyboard or mute control failed");shot("options-muted");key(SDLK_F10,KMOD_NONE);
 click({X+35,Y+90},false);click({X+185,Y+225},false);click({X+250,Y+65},false);click({X+250,Y+193},false);
 check(settings_.heroSpeed==0&&settings_.scrollSpeed==2&&!settings_.showPath&&!settings_.townOutlines,"Original client options did not change");shot("options-changed");
 ClientSettings saved;std::string error;check(saved.load(saveDir_/"client-options.json",error)&&saved==settings_,"Client options did not persist");key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Adventure&&game.toJson()==before,"Closing options changed game state");
 render();Point goal=mapScreen({4,21});click(goal,false);check(marching_&&!route_.empty(),"Disabling move paths did not start movement on the first click");shot("adventure-no-move-path");
 auto mapFile=directory/"adventure-no-move-path.png";SDL_Surface*source=IMG_Load(mapFile.string().c_str());check(source,"Map pixel capture could not load");SDL_Surface*map=SDL_ConvertSurfaceFormat(source,SDL_PIXELFORMAT_RGBA32,0);SDL_FreeSurface(source);check(map,"Map pixel format conversion failed");
 int colored=0;double scale=map->w/800.;for(int y=int(100*scale);y<int(500*scale);y++)for(int x=int(100*scale);x<int(500*scale);x++){auto*p=static_cast<Uint8*>(map->pixels)+y*map->pitch+x*4;if(int(p[0])+p[1]+p[2]>50)colored++;}SDL_FreeSurface(map);check(colored>20000*scale*scale,"Map content disappeared after closing system options");
 auto bytes=[](const std::filesystem::path&p){std::ifstream input(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(input),{});};
 auto repeated=saveDir_/"repeated-map.png";check(screenshot(repeated)&&bytes(repeated)==bytes(mapFile),"Reading the retained frame changed its pixels");render();check(screenshot(repeated)&&bytes(repeated)==bytes(mapFile),"Consecutive map frames changed without a state change");
 marching_=false;route_.clear();destination_.reset();
 scene=Scene::Town;key(SDLK_o,KMOD_NONE);shot("options-town");key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Town,"Default Return did not close town options");
 armyReturn_=Scene::Town;splitReturn_=Scene::Army;scene=Scene::Split;check(audioTheme()=="town.castle","Auxiliary split lost its town music context");
 game.startBattle(15);scene=Scene::Battle;check(audioTheme()=="combat.1","Battle did not select combat music");openOptions();check(audioTheme()=="combat.1","Battle options interrupted combat music");
 scene=Scene::Menu;check(audioTheme()=="menu.main","Main menu retained the suspended battle theme");
 game.battle.reset();game.report.emplace();scene=Scene::Report;game.result=1;game.report->won=true;check(audioTheme()=="report.scenario-win","Scenario victory used ordinary battle music");
 game.result=-1;game.report->won=false;check(audioTheme()=="report.ultimate-loss","Scenario defeat used ordinary battle music");
 game.result=0;check(audioTheme()=="report.loss","Ordinary battle defeat used scenario music");game=Game::create();settings_=ClientSettings{};
 // Exercise the native window-point boundary, including letterboxing and a
 // stale release position observed with SDL2-compat on the Retina desktop.
 auto raw=[this](Point logical){int w=0,h=0;SDL_GetWindowSize(window_,&w,&h);auto layout=displayLayout(w,h,w,h,scene==Scene::Adventure);return Point{int(std::lround(layout.pointX+(logical.x+.5)*layout.pointScale)),int(std::lround(layout.pointY+(logical.y+.5)*layout.pointScale))};};
 auto nativeMouse=[this](Uint32 type,Point p){SDL_Event e{};e.type=type;if(type==SDL_MOUSEMOTION){e.motion.x=p.x;e.motion.y=p.y;}else{e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x;e.button.y=p.y;}handleNativeEvent(e);};
 auto nativeClick=[&](Point p){nativeMouse(SDL_MOUSEBUTTONDOWN,raw(p));nativeMouse(SDL_MOUSEBUTTONUP,{1,1});};
 for(auto size:{Point{1280,720},Point{640,480}}){
  scene=Scene::Town;SDL_SetWindowSize(window_,size.x,size.y);SDL_Event resized{};resized.type=SDL_WINDOWEVENT;resized.window.event=SDL_WINDOWEVENT_SIZE_CHANGED;handleNativeEvent(resized);
  auto center=windowPoint({size.x/2,size.y/2});check(center==Point{400,300},"Resized native pointer center changed");
  if(size.x==1280)check(windowPoint({0,360}).x<0,"Letterbox mapped into the game canvas");
  scene=Scene::Adventure;render();nativeMouse(SDL_MOUSEMOTION,raw({100,100}));nativeClick({727+canvasLogicalSize().x-800,308});
  check(scene==Scene::Options&&optionsReturn_==Scene::Adventure,"Native computer button used a stale hover or release position");
  shot(size.x==1280?"options-wide":"options-small");
  nativeMouse(SDL_MOUSEBUTTONDOWN,raw({190,430}));nativeMouse(SDL_MOUSEMOTION,raw({292,430}));nativeMouse(SDL_MOUSEBUTTONUP,{1,1});
  check(settings_.music==5&&optionsDragging_<0&&!mouseLeftDown_,"Native volume drag lost its motion or release position");
  key(SDLK_ESCAPE,KMOD_NONE);render();auto state=game.toJson();nativeMouse(SDL_MOUSEBUTTONDOWN,{0,0});nativeMouse(SDL_MOUSEBUTTONUP,{0,0});
  check(scene==Scene::Adventure&&game.toJson()==state,"Outside native click changed the game");
 }
 SDL_SetWindowSize(window_,1000,750);nativePointer_.reset();scene=Scene::Town;render();
 int armyCount=game.hero.army[0].count;nativeMouse(SDL_MOUSEBUTTONDOWN,raw({332,516}));nativeMouse(SDL_MOUSEMOTION,raw({332,418}));nativeMouse(SDL_MOUSEBUTTONUP,{1,1});
 check(game.hero.army[0].empty()&&game.town.garrison[0].count==armyCount&&!armyDrag_,"Native Retina army drag failed");
 render();nativeMouse(SDL_MOUSEBUTTONDOWN,raw({332,418}));nativeMouse(SDL_MOUSEMOTION,raw({332,516}));nativeMouse(SDL_MOUSEBUTTONUP,{1,1});
 check(game.hero.army[0].count==armyCount&&game.town.garrison[0].empty(),"Native army return drag changed unit counts");
 nativeMouse(SDL_MOUSEBUTTONDOWN,raw({332,516}));SDL_Event lostNative{};lostNative.type=SDL_WINDOWEVENT;lostNative.window.event=SDL_WINDOWEVENT_FOCUS_LOST;handleNativeEvent(lostNative);
 check(!nativePointer_&&!armyDrag_&&!mouseLeftDown_,"Native focus loss retained pointer capture");
 settings_=ClientSettings{};scene=Scene::Town;
}
}
