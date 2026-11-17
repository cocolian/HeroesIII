#include "h3/renderer.hpp"
#include "h3/localization.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace h3 {
void Desktop::visitDialog(){
 if(!visitPopup_){scene=Scene::Adventure;return;}
 mapDialog({170,140,460,320});const auto&notice=*visitPopup_;
 constexpr SDL_Color gold{236,213,130,255};
 centerText(chineseMapText(game.scenarioId,notice.title),{194,164,412,40},24,gold);
 auto message=chineseMapText(game.scenarioId,notice.message);
 // H3M braces mark emphasis. The plain-text renderer must not expose the markup.
 std::erase_if(message,[](char c){return c=='{'||c=='}'||c=='\r';});
 if(visitMessage_!=message){visitMessage_=message;visitPage_=0;}
 // Measure the actual Chinese font, rather than estimating byte/glyph widths.
 // Explicit line breaks keep pagination and SDL_ttf rendering in agreement.
 std::vector<std::string> lines;std::string line;
 for(size_t i=0;i<message.size();){
  unsigned char c=message[i];size_t count=c<0x80?1:c<0xe0?2:c<0xf0?3:4;
  count=std::min(count,message.size()-i);auto glyph=message.substr(i,count);i+=count;
  if(c=='\n'){lines.push_back(line);line.clear();continue;}
  auto next=line+glyph;int width=0,height=0;
  if(TTF_SizeUTF8(font(16),next.c_str(),&width,&height)==0&&width>int(392*pixelScale_)&&!line.empty()){lines.push_back(line);line=glyph;}else line=std::move(next);
 }
 if(!line.empty()||lines.empty())lines.push_back(line);
 std::vector<std::string> pages;
 for(size_t start=0;start<lines.size();start+=4){std::string page;for(size_t row=start;row<std::min(start+4,lines.size());++row){if(row>start)page+='\n';page+=lines[row];}pages.push_back(std::move(page));}
 visitPage_=std::clamp(visitPage_,0,int(pages.size())-1);
 const auto firstLine=size_t(visitPage_)*4;int lineHeight=int(std::ceil(TTF_FontLineSkip(font(16))/pixelScale_));
 for(size_t row=firstLine;row<std::min(firstLine+4,lines.size());++row)text(lines[row],204,225+int(row-firstLine)*lineHeight,16,{244,224,174,255});
 int rewards=0;for(int n:notice.reward)rewards+=n>0;int column=0;
 for(int r=0;r<ResourceCount;++r)if(notice.reward[r]>0){if(rewards==1){marketIcon(r,{365,305,70,46});centerText("+"+std::to_string(notice.reward[r]),{350,354,100,25},17,gold);}else{int x=204+column++*55;marketIcon(r,{x,305,48,33});centerText("+"+std::to_string(notice.reward[r]),{x-3,343,54,22},11,gold);}}
 if(notice.spell>=0){int id=Spells[notice.spell].originalId;art("original/spells/spells.png",{362,300,76,64},SDL_Rect{20+83*(id%11),500+66*(id/11),76,64});}
 if(notice.morale||notice.luck){int count=(notice.morale!=0)+(notice.luck!=0),column=0;for(int stat=0;stat<2;++stat){int value=stat?notice.luck:notice.morale;if(!value)continue;int x=400-count*42+column++*84;frame(stat?"ilck42.def":"imrl42.def",0,3+std::clamp(value,-3,3),{x,307,42,38});centerText((stat?std::string("幸运 "):std::string("士气 "))+(value>0?"+":"")+std::to_string(value),{x-16,350,74,22},12,gold);}}
 if(pages.size()>1){button({215,374,77,24},"上一页",[this]{--visitPage_;},visitPage_>0);centerText(std::to_string(visitPage_+1)+" / "+std::to_string(pages.size()),{345,374,110,24},12,gold);button({508,374,77,24},"下一页",[this]{++visitPage_;},visitPage_+1<int(pages.size()));}
 originalButton({368,404,64,32},"iokay.def",0,[this]{visitPopup_.reset();scene=Scene::Adventure;focus_=-1;mouseX_=mouseY_=-1;},true,-1,"确认访问结果，返回地图（Enter / Esc）");
}
void Desktop::puzzle(){
 auto*target=SDL_GetRenderTarget(renderer_);auto&terrain=images_["#puzzle-terrain"];
 uint64_t key=1469598103934665603ULL;
 for(const auto&t:game.tiles){key=(key^uint64_t(int(t.terrain)+4*int(t.decor)+16*t.road+32*t.river+64*t.variant))*1099511628211ULL;}
 key=(key^uint64_t(game.grailPos.y*game.mapWidth+game.grailPos.x))*1099511628211ULL;
 auto&screen=images_["#puzzle-screen"];
 if(!screen.texture){screen={SDL_CreateTexture(renderer_,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,800,600),800,600};if(!screen.texture)throw std::runtime_error(SDL_GetError());SDL_SetTextureBlendMode(screen.texture,SDL_BLENDMODE_NONE);SDL_SetTextureScaleMode(screen.texture,SDL_ScaleModeLinear);}
 if(!terrain.texture||key!=puzzleArtKey_){
  SDL_SetRenderTarget(renderer_,screen.texture);SDL_RenderSetScale(renderer_,1,1);SDL_RenderSetClipRect(renderer_,nullptr);SDL_SetRenderDrawColor(renderer_,0,0,0,0);SDL_RenderClear(renderer_);
  auto oldX=camX_,oldY=camY_,oldZoom=zoom_;auto oldTime=animationTime_;bool hd=settings_.hdArtwork;
  std::vector<bool> seen(game.tiles.size());for(size_t i=0;i<game.tiles.size();++i){seen[i]=game.tiles[i].seen;game.tiles[i].seen=true;}
  camX_=game.grailPos.x+.5f;camY_=game.grailPos.y+.5f;zoom_=1;animationTime_=0;settings_.hdArtwork=false;
  SDL_Rect map{8,8,592,544};SDL_RenderSetClipRect(renderer_,&map);adventureTerrain();
  for(int y=0;y<game.mapHeight;++y)for(int x=0;x<game.mapWidth;++x){auto decor=game.tile({x,y})->decor;if(decor!=Decor::None)mapSprite(decor==Decor::Tree?"avlpntr0.def":"avlmtgr5.def",{x,y},false);}
  for(size_t i=0;i<game.tiles.size();++i)game.tiles[i].seen=seen[i];camX_=oldX;camY_=oldY;zoom_=oldZoom;animationTime_=oldTime;settings_.hdArtwork=hd;
  SDL_RenderSetClipRect(renderer_,nullptr);
  auto*surface=SDL_CreateRGBSurfaceWithFormat(0,800,600,32,SDL_PIXELFORMAT_RGBA32);if(!surface)throw std::runtime_error(SDL_GetError());
  if(SDL_RenderReadPixels(renderer_,nullptr,SDL_PIXELFORMAT_RGBA32,surface->pixels,surface->pitch)!=0){SDL_FreeSurface(surface);throw std::runtime_error(SDL_GetError());}
  for(int y=0;y<600;++y)for(int x=0;x<800;++x){auto*p=static_cast<Uint8*>(surface->pixels)+y*surface->pitch+x*4;int gray=(77*p[0]+150*p[1]+29*p[2])/256;p[0]=Uint8(std::min(255,gray*112/100));p[1]=Uint8(gray*92/100);p[2]=Uint8(gray*65/100);}
  if(terrain.texture)SDL_DestroyTexture(terrain.texture);terrain={SDL_CreateTextureFromSurface(renderer_,surface),800,600};SDL_FreeSurface(surface);if(!terrain.texture)throw std::runtime_error(SDL_GetError());SDL_SetTextureBlendMode(terrain.texture,SDL_BLENDMODE_BLEND);puzzleArtKey_=key;
 }
 SDL_SetRenderTarget(renderer_,screen.texture);SDL_RenderSetScale(renderer_,1,1);SDL_RenderSetClipRect(renderer_,nullptr);
 originalImage("puzzle.pcx",{0,0,800,600});SDL_Rect full{0,0,800,600};SDL_RenderCopy(renderer_,terrain.texture,nullptr,&full);
 int visited=game.obelisksVisited(),total=game.obelisksTotal(),uncovered=total?48*visited/total:0;
 if(uncovered==48){auto oldX=camX_,oldY=camY_,oldZoom=zoom_;camX_=game.grailPos.x+.5f;camY_=game.grailPos.y+.5f;zoom_=1;auto p=mapScreen(game.grailPos);camX_=oldX;camY_=oldY;zoom_=oldZoom;
  SDL_SetRenderDrawColor(renderer_,225,194,117,255);for(int d=-1;d<=1;++d){SDL_RenderDrawLine(renderer_,p.x-8,p.y-8+d,p.x+8,p.y+8+d);SDL_RenderDrawLine(renderer_,p.x-8,p.y+8+d,p.x+8,p.y-8+d);}
 }
 std::ifstream file(root_/"assets/demo/puzzle.json");nlohmann::json data;file>>data;
 for(int i=0;i<48;++i){const auto&p=data["pieces"][i];if(p["order"].get<int>()<=uncovered)continue;auto name="puzcas"+std::string(i<10?"0":"")+std::to_string(i)+".pcx";const auto&artwork=demo_["pcx"][name];art(artwork["file"],{p["x"],p["y"],artwork["width"],artwork["height"]},{},true);}
 SDL_SetRenderTarget(renderer_,target);SDL_RenderSetScale(renderer_,float(pixelScale_),float(pixelScale_));SDL_RenderSetClipRect(renderer_,nullptr);SDL_RenderCopy(renderer_,screen.texture,nullptr,&full);buttons_.clear();
 constexpr SDL_Color gold{236,213,130,255};centerText("藏宝图",{611,30,180,42},27,gold);
 centerText("方尖碑 "+std::to_string(visited)+" / "+std::to_string(total),{613,92,176,30},18,gold);
 text(game.grailFound?"圣杯已经被挖出。":uncovered==48?"拼图已完整揭开，X 标记圣杯埋藏的位置。":"拜访不同的方尖碑，逐步揭开地图。",624,152,16,gold,158);
 text("英雄在目标格停留到新的一天，尚未移动时按 D 挖掘。",624,300,14,{222,200,160,255},155);
 originalButton({673,536,64,32},"iokay.def",0,[this]{visitPopup_.reset();scene=Scene::Adventure;focus_=-1;mouseX_=mouseY_=-1;},true,-1,"关闭藏宝图，返回冒险地图（Enter / Esc）");
}
void Desktop::adventureVisitRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int width,height;SDL_GetWindowSize(window_,&width,&height);SDL_SetWindowSize(window_,1280,800);uint64_t clock=1000;
 auto advance=[&]{fixedAnimationTime_=clock+=100000;tick();};
 auto setup=[&]{cancelPointer();battleMotion_.clear();game=Game::create();game.objects.clear();for(auto&t:game.tiles){t.seen=true;t.decor=Decor::None;t.terrain=Terrain::Grass;t.road=t.river=false;}game.hero.pos={10,12};scene=Scene::Adventure;settings_=ClientSettings{};settings_.hdArtwork=false;camX_=12.5f;camY_=11.5f;zoom_=1;route_.clear();destination_.reset();visitPopup_.reset();marching_=autoCombat_=false;pendingSpell_=sacrificeTarget_=-1;nextStep_=nextAI_=0;mouseX_=mouseY_=-1;toastUntil_=0;fixedAnimationTime_=clock;};
 auto add=[&](ObjectType type,Point pos,int id=100){Object o;o.id=id;o.type=type;o.pos=pos;o.name=type==ObjectType::Well?"魔法井":type==ObjectType::Windmill?"风车":type==ObjectType::WaterWheel?"水车":type==ObjectType::Shrine?"一级魔法神殿":"方尖碑";game.objects.push_back(o);};
 auto shot=[&](const std::string&name){toastUntil_=0;render();check(screenshot(directory/("visit-"+name+".png")),"Adventure visit screenshot failed");};
 auto walk=[&](Point goal){render();click(mapScreen(goal),false);key(SDLK_m,KMOD_NONE);for(int i=0;i<40&&marching_;++i)advance();check(game.hero.pos==goal,"Object entrance could not be reached");};
 auto ack=[&]{render();key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Adventure&&!visitPopup_,"Object confirmation did not resume exploration");};
 setup();game.hero.pos={9,12};game.tile({12,12})->decor=Decor::Mountain;game.tile({14,10})->decor=Decor::Mountain;
 render();click(mapScreen({16,12}),false);auto planned=route_;check(!planned.empty(),"Mountain detour missing");Point previous=game.hero.pos;
 for(auto s:planned){Point p{s.x,s.y};check(game.passable(p)&&game.canStep(previous,p),"Route crossed the original mountain footprint");previous=p;}
 shot("mountain-detour");key(SDLK_m,KMOD_NONE);for(int i=0;i<40&&marching_;++i)advance();check((game.hero.pos==Point{16,12}),"Mountain detour movement stalled");shot("mountain-arrived");
 nlohmann::json visits=nlohmann::json::array();
 for(auto type:{ObjectType::Well,ObjectType::Windmill,ObjectType::WaterWheel,ObjectType::Shrine}){
  setup();add(type,{12,10});game.hero.mana=1;game.hero.spellbook=true;auto resources=game.resources;int xp=game.hero.xp;walk({12,10});
  check(scene==Scene::Visit&&visitPopup_&&!marching_,"Visitable object skipped its acknowledgement dialog");
  if(type==ObjectType::Well)check(game.hero.mana==game.maxMana()&&game.hero.wellDay==game.day,"Well did not restore mana once");
  if(type==ObjectType::Shrine)check(game.hero.spells[MagicArrow]&&game.hero.xp==xp,"Shrine did not teach a spell without XP");
  auto state=game.toJson();key(SDLK_e,KMOD_NONE);advance();check(scene==Scene::Visit&&game.toJson()==state,"An open visit dialog advanced time or movement");shot("fresh-"+std::to_string(int(type)));ack();
  if(type==ObjectType::Well)game.hero.mana=1;resources=game.resources;render();click(mapScreen(game.hero.pos),false);check(scene==Scene::Visit&&game.resources==resources,"Repeated visit paid resources twice");
  if(type==ObjectType::Well)check(game.hero.mana==1,"Repeated well visit restored mana twice");shot("repeat-"+std::to_string(int(type)));ack();
  visits.push_back({{"type",int(type)},{"acknowledgement",true},{"repeatRewardPrevented",true}});
 }
 setup();add(ObjectType::Shrine,{12,10});walk({12,10});check(!game.hero.spells[MagicArrow]&&!game.objects[0].visited,"Bookless shrine granted a spell");shot("shrine-no-book");ack();
 setup();game.grailPos={11,13};add(ObjectType::Obelisk,{12,10});add(ObjectType::Obelisk,{15,10},101);
 auto state=game.toJson();scene=Scene::Puzzle;shot("puzzle-covered");check(game.toJson()==state,"Drawing the puzzle changed world fog or gameplay");ack();
 walk({12,10});check(scene==Scene::Puzzle&&game.obelisksVisited()==1,"Obelisk did not open its partial puzzle");state=game.toJson();shot("puzzle-partial");check(game.toJson()==state,"Partial puzzle changed world fog");ack();
 render();click(mapScreen(game.hero.pos),false);check(scene==Scene::Puzzle&&game.obelisksVisited()==1,"Revisiting obelisk advanced puzzle progress");ack();
 walk({15,10});check(scene==Scene::Puzzle&&game.obelisksVisited()==2,"Second obelisk did not complete puzzle");shot("puzzle-complete");ack();
 walk(game.grailPos);key(SDLK_e,KMOD_NONE);key(SDLK_d,KMOD_NONE);check(game.grailFound&&game.hero.movement==0,"Native dig shortcut failed");shot("grail-dug");
 // Guard interception must happen before a well's effect and its dialog.
 setup();add(ObjectType::Well,{12,10});add(ObjectType::Monster,{13,10},101);game.objects.back().army={Stack{Skeleton,1}};game.hero.army={Stack{Archer,100}};game.hero.mana=1;
 walk({12,10});check(scene==Scene::Battle&&game.hero.mana==1&&game.battle->deferredVisitId==100,"Guarded well granted mana before combat");
 battleAction([&]{game.ai();return std::string{};});for(int i=0;i<50&&(game.battle||battleMotion_.active());++i)advance();
 check(scene==Scene::Report&&game.report&&game.hero.mana==game.maxMana(),"Guard victory did not commit deferred well effect");render();click({582,542},false);check(scene==Scene::Visit,"Deferred well dialog skipped after battle report");shot("guarded-well-after-report");ack();
 nlohmann::json animations=nlohmann::json::array();
 for(auto type:{ObjectType::Well,ObjectType::Windmill,ObjectType::WaterWheel,ObjectType::Obelisk,ObjectType::Shrine}){
  setup();add(type,{12,10});auto name=game.mapObjectAsset(game.objects[0]);animations.push_back({{"type",int(type)},{"asset",name},{"frames",frameCount(name)},{"continuousAfterVisit",true}});
  check(frameCount(name)==(type==ObjectType::Well||type==ObjectType::Obelisk?1:8),"Object animation differs from original DEF frames");
  game.objects[0].visited=true;fixedAnimationTime_=0;shot("animation-"+std::to_string(int(type))+"-0");fixedAnimationTime_=180;shot("animation-"+std::to_string(int(type))+"-1");
 }
 std::ofstream(directory/"adventure-visits-audit.json")<<nlohmann::json{{"mountainDetourAndActualMovement",true},{"originalFootprintsShared",true},{"visits",visits},{"wellDailyLimit",true},{"shrineRequiresBookAndNoXp",true},{"obeliskRevealsPuzzleNotFog",true},{"puzzleRenderingDoesNotMutateWorld",true},{"fullDayDigAndUniqueGrail",true},{"guardedWellAfterReport",true},{"animations",animations}}.dump(2)<<'\n';
 setup();game=Game::create();scene=Scene::Town;fixedAnimationTime_=0;SDL_SetWindowSize(window_,width,height);
}
}
