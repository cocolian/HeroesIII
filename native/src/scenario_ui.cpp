#include "h3/renderer.hpp"
#include <algorithm>
#include <fstream>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace h3 {
namespace {
constexpr SDL_Color GoldColor{236,213,130,255},Muted{189,171,135,255},Green{125,195,111,255};
bool contains(SDL_Rect r,Point p){return p.x>=r.x&&p.y>=r.y&&p.x<r.x+r.w&&p.y<r.y+r.h;}
std::string shortened(const std::string&s,size_t count){size_t glyphs=0;for(size_t i=0;i<s.size();++i)if((static_cast<unsigned char>(s[i])&0xc0)!=0x80&&glyphs++==count)return s.substr(0,i)+"…";return s;}
}
void Desktop::selectMap(int index){
 scenarioHero_=-1;scenarioVideoPreset_=false;selectedMap_=std::clamp(index,0,int(mapLibrary_["entries"].size())-1);mapPage_=selectedMap_/6;mapLevel_=0;focus_=-1;
}
void Desktop::scenario(){
 title("新游戏 · 选择起始城镇",Scene::Menu);
 text("下方图片选择城镇阵营；更换地图请点左下方「选择地图」。",35,57,11,Muted);
 for(int f=0;f<FactionCount;++f){
  SDL_Rect r{35+f%3*245,73+f/3*143,235,133};panel(r);
  art("original/"+Factions[f].background,{r.x+3,r.y+3,229,96},{},false);
  button({r.x+3,r.y+101,229,29},Factions[f].name+(f==scenarioFaction_?" · 已选择":""),[this,f]{scenarioFaction_=f;scenarioHero_=-1;focus_=-1;},true,false,"选择"+Factions[f].name+"作为起始城镇；整张图片和文字均可点击（"+std::to_string(f+1)+"）");
  // The caption retains its layout, while the full illustrated card owns input.
  buttons_.back().rect=r;
  if(f==scenarioFaction_||contains(r,{mouseX_,mouseY_}))outline({r.x-2,r.y-2,r.w+4,r.h+4},f==scenarioFaction_?GoldColor:Muted);
 }
 bool playable=mapLibrary_["entries"][selectedMap_].value("playable",false);
 auto name=mapLibrary_["entries"][selectedMap_]["title"].get<std::string>();
 text("当前地图："+shortened(name,28)+(selectedMap_==0?"（可游玩）":playable?"（原图探索）":"（仅预览）"),35,511,12,playable?Green:GoldColor);
 text(selectedMap_==0?"边境练习地图，支持九种起始城镇。":mapLibrary_["entries"][selectedMap_]["id"]=="seven-lakes-1"?"按所选难度分配资源，并生成所选起始英雄。":"按原图开局；阵营选项仅影响随机城镇。",35,531,11,Muted);
 button({525,510,225,40},playable?(selectedMap_==0?"开始游戏":"开始原图探索"):"暂不支持开始",[this]{startScenario();},playable,false,selectedMap_==0?"使用所选城镇开始边境练习地图（Enter）":"载入真实地图、原始对象与开局；可探索、拾取、占矿和战斗。部分剧情与英雄能力尚未完整支持（Enter）");
 if(mapLibrary_["entries"][selectedMap_]["id"]=="dead-and-buried-1")button({275,551,235,27},scenarioVideoPreset_?"视频开局 · 已选":"视频开局 · 元素城 / 200%",[this]{scenarioVideoPreset_=true;scenarioFaction_=8;},true,false,"按视频选用元素城与拉克斯、200% 零资源开局；不同随机种子和未实现的战术特长仍会影响比较");
 if(selectedMap_>0&&mapLibrary_["entries"][selectedMap_]["id"]!="dead-and-buried-1"){Hero selected;if(scenarioHero_>=0)heroDefaults(selected,scenarioHero_);button({275,551,235,27},scenarioHero_<0?"起始英雄 · 默认":"起始英雄 · "+selected.name,[this]{int next=scenarioHero_<0?0:scenarioHero_-scenarioFaction_*16+1;scenarioHero_=next>=16?-1:scenarioFaction_*16+next;},true,false,"切换随机开局的起始英雄；地图预设英雄会保留原设定");}
 const char*levels[]={"简单 · 50%","普通 · 100%","困难 · 130%","专家 · 160%","不可能 · 200%"};button({525,551,225,27},"难度："+std::string(levels[scenarioVideoPreset_?4:scenarioDifficulty_]),[this]{scenarioVideoPreset_=false;scenarioDifficulty_=(scenarioDifficulty_+1)%5;},true,false,"切换原版五档难度；开局资源和电脑的收入修正随难度变化");
 button({35,551,220,27},"选择地图 · 已下载地图",[this]{scene=Scene::Maps;mapPage_=selectedMap_/6;focus_=-1;},true,false,"打开地图列表：1 张练习地图及 15 张已下载地图的原图探索入口");
}
void Desktop::mapLibrary(){
 title("选择地图 · 已下载地图",Scene::Scenario);
 text("已接入多英雄及基础电脑回合；复杂剧情、兵种能力和攻城仍有缺失。",35,56,11,Muted);
 const auto&entries=mapLibrary_["entries"];int pages=(int(entries.size())+5)/6;
 for(int row=0;row<6;++row){
  int index=mapPage_*6+row;if(index>=int(entries.size()))break;const auto&item=entries[index];
  SDL_Rect r{30,82+row*58,310,52};panel(r);if(index==selectedMap_)outline({r.x-2,r.y-2,r.w+4,r.h+4},GoldColor);
  text(shortened(item["title"],36),r.x+9,r.y+6,12,GoldColor);
  text((index==0?"可游玩 · ":item.value("playable",false)?"原图探索 · ":"预览 · ")+shortened(item["variant"],36),r.x+9,r.y+29,10,item.value("playable",false)?Green:Muted);
  button(r,"",[this,index]{selectMap(index);},true,true,item["title"].get<std::string>()+"："+(index==0?"可开始练习地图":"选择真实地图并开始探索；仍有未实现的原版规则"));
 }
 button({35,443,80,28},"上一页",[this]{--mapPage_;focus_=-1;},mapPage_>0);
 centerText(std::to_string(mapPage_+1)+" / "+std::to_string(pages),{119,444,126,26},13);
 button({250,443,80,28},"下一页",[this]{++mapPage_;focus_=-1;},mapPage_+1<pages);
 text("1 张练习图可游玩\n15 张已下载地图可进入原图探索",35,490,12,Muted,300);
 panel({360,77,409,434});const auto&item=entries[selectedMap_];
 text(shortened(item["title"],42),377,89,16,GoldColor,374);
 text(std::to_string(item["width"].get<int>())+"×"+std::to_string(item["height"].get<int>())+" · "+(item["levels"].get<int>()==2?"双层":"单层")+" · "+shortened(item["author"],24),377,116,10,Muted);
 SDL_Rect preview{416,137,288,288};box(preview,{13,13,11,255});
 if(selectedMap_==0){
  static const auto practice=Game::create();
  for(int y=0;y<MapH;++y)for(int x=0;x<MapW;++x){const auto&t=*practice.tile({x,y});SDL_Color color=t.terrain==Terrain::Water?SDL_Color{35,77,135,255}:t.terrain==Terrain::Sand?SDL_Color{210,182,120,255}:t.terrain==Terrain::Dirt?SDL_Color{116,82,48,255}:SDL_Color{64,114,48,255};if(t.river)color={62,117,158,255};if(t.road)color={160,127,76,255};box({preview.x+x*8,preview.y+32+y*8,8,8},color);}
 }else{
  const auto&previews=item["previews"];mapLevel_=std::clamp(mapLevel_,0,int(previews.size())-1);art(previews[mapLevel_],preview,{},false);
 }
 outline(preview,{140,115,65,255});
 for(int level=0;level<item["levels"].get<int>();++level)button({377+level*98,435,92,25},(level==0?"地上":"地下")+std::string(level==mapLevel_?" · 当前":""),[this,level]{mapLevel_=level;focus_=-1;},true,false,level==0?"预览地上层地形；图中不包含物件和事件":"预览地下层地形；图中不包含物件和事件");
 text(selectedMap_==0?"可以开始游戏":"可开始原图探索",377,470,13,Green);
 text(selectedMap_==0?"返回上一页选择城镇后开始。":"探索、拾取、占矿与战斗；部分剧情仍有缺失。",377,491,11,Muted,373);
#ifdef __IPHONEOS__
 button({35,549,220,31},"地图文件已内置",[]{},false,false,"iPad 不支持在访达中打开应用内地图目录");
#else
 button({35,549,220,31},"打开社区地图目录",[this]{
  auto path=(root_/"maps/community").string();std::string url="file://";constexpr char hex[]="0123456789ABCDEF";
  for(unsigned char c:path){if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='-'||c=='_'||c=='.')url+=char(c);else{url+='%';url+=hex[c>>4];url+=hex[c&15];}}
  if(SDL_OpenURL(url.c_str())!=0)say("无法打开地图目录："+std::string(SDL_GetError()));
 },true,false,"在访达中查看安装包内已下载的原始 H3M 文件");
#endif
 button({535,543,217,34},selectedMap_==0?"使用练习地图":"选择此地图",[this]{scene=Scene::Scenario;focus_=-1;},item.value("playable",false),false,selectedMap_==0?"选择边境练习图，返回起始城镇选择":"使用这张真实地图，返回新游戏后点击「开始原图探索」");
}
void Desktop::startScenario(){
 const auto&entry=mapLibrary_["entries"][selectedMap_];if(!entry.value("playable",false))return;
 Game candidate;if(selectedMap_==0)candidate=Game::create(1999,scenarioFaction_);else{std::string error;if(!candidate.importScenario(root_/entry["scenarioFile"].get<std::string>(),error,scenarioFaction_,scenarioVideoPreset_?131:scenarioHero_,scenarioVideoPreset_?4:scenarioDifficulty_)){say("地图载入失败："+error);return;}}
 heroListPage_=townListPage_=0;game=std::move(candidate);visitPopup_.reset();heroDirection_={1,0};battleMotion_.clear();autoCombat_=false;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;scene=Scene::Adventure;route_.clear();destination_.reset();marching_=false;pendingSpell_=sacrificeTarget_=-1;buildPage_=0;sourceSlot_=-1;recruitUnit_=game.town.faction*14;toastUntil_=0;toast_.clear();mouseX_=mouseY_=-1;scrollTick_=0;mouseLeftDown_=minimapDragging_=drag_=false;armyDrag_.reset();focus_=-1;nativePointer_.reset();persist();settle();
}
void Desktop::scenarioPreview(const std::filesystem::path&directory){
 std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";scenarioRenderTests(directory);std::cout<<"Scenario cards and downloaded map library passed\n";
}
void Desktop::scenarioRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int oldW,oldH;SDL_GetWindowSize(window_,&oldW,&oldH);auto previous=settings_;uint64_t clock=1000;
 auto reset=[&]{cancelPointer();battleMotion_.clear();game=Game::create();scene=Scene::Scenario;settings_=ClientSettings{};scenarioFaction_=0;selectMap(0);visitPopup_.reset();route_.clear();destination_.reset();marching_=autoCombat_=false;focus_=-1;mouseX_=mouseY_=-1;toastUntil_=0;fixedAnimationTime_=clock+=1000;};
 auto raw=[&](Point p){int w,h;SDL_GetWindowSize(window_,&w,&h);auto layout=displayLayout(w,h,w,h,false);return Point{int(std::lround(layout.pointX+(p.x+.5)*layout.pointScale)),int(std::lround(layout.pointY+(p.y+.5)*layout.pointScale))};};
 auto pointer=[&](Uint32 type,Point p){p=raw(p);SDL_Event e{};e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x;e.button.y=p.y;handleNativeEvent(e,true);};
 auto tap=[&](Point p){pointer(SDL_MOUSEBUTTONDOWN,p);pointer(SDL_MOUSEBUTTONUP,p);};
 auto shot=[&](const std::string&name){toastUntil_=0;render();check(screenshot(directory/("selection-"+name+".png")),"Scenario screenshot failed");};
 nlohmann::json selections=nlohmann::json::array();
 for(auto size:{Point{640,480},Point{1512,982},Point{2390,1658}}){
  SDL_SetWindowSize(window_,size.x,size.y);reset();
  for(int faction=0;faction<FactionCount;++faction){
   scene=Scene::Scenario;render();auto before=game.toJson();Point picture{35+faction%3*245+110,73+faction/3*143+45};
   pointer(SDL_MOUSEBUTTONDOWN,picture);check(scenarioFaction_==(faction?faction-1:0),"Card selection triggered on pointer down");pointer(SDL_MOUSEBUTTONUP,picture);
   check(scenarioFaction_==faction&&game.toJson()==before,"Full card image did not select its faction or changed the game");
   if(size.x==1512)shot("faction-"+std::to_string(faction));
   render();tap({630,530});check(scene==Scene::Adventure&&game.town.faction==faction&&game.hero.army[0].unit==faction*14,"Selected faction was lost when starting the map");
   selections.push_back({{"window",{size.x,size.y}},{"faction",faction},{"imageClick",true},{"startedWithCorrectTownAndArmy",true}});
  }
 }
 SDL_SetWindowSize(window_,1280,800);reset();render();pointer(SDL_MOUSEBUTTONDOWN,{390,120});pointer(SDL_MOUSEBUTTONUP,{635,120});check(scenarioFaction_==0,"Dragging off a card selected another faction");
 tap({390,319});check(scenarioFaction_==4,"Caption click no longer selects a town");
 key(SDLK_9,KMOD_NONE);check(scenarioFaction_==8,"Number shortcut did not select the ninth faction");
 render();tap({120,562});check(scene==Scene::Maps,"Downloaded-map entry is missing from town selection");
 shot("library-practice");auto state=game.toJson();nlohmann::json maps=nlohmann::json::array();
 check(mapLibrary_["entries"].size()==16,"Runtime catalog omitted downloaded maps");
 for(int index=1;index<int(mapLibrary_["entries"].size());++index){
  mapPage_=index/6;render();tap({130,82+(index%6)*58+22});check(selectedMap_==index,"Map row could not be selected");
  const auto&entry=mapLibrary_["entries"][index];auto file=root_/entry["file"].get<std::string>();check(std::filesystem::is_regular_file(file),"Downloaded H3M was not included with the game");
  for(int level=0;level<entry["levels"].get<int>();++level){render();tap({410+level*98,447});check(mapLevel_==level,"Map layer switch did not select its level");shot("map-"+std::to_string(index)+"-"+std::to_string(level));}
  render();check(buttons_.back().enabled,"Imported community map remained disabled");tap({630,560});check(scene==Scene::Scenario&&game.toJson()==state,"Map selection changed gameplay before starting");render();tap({630,530});
  for(int acknowledgement=0;scene==Scene::Visit&&acknowledgement<5;++acknowledgement){render();key(SDLK_RETURN,KMOD_NONE);}
  check(scene==Scene::Adventure&&game.imported()&&game.scenarioId==entry["id"].get<std::string>()&&game.mapWidth==entry["width"]&&game.mapHeight==entry["height"]&&game.validate(),"Community start failed or substituted the practice map");shot("started-map-"+std::to_string(index));
  maps.push_back({{"id",entry["id"]},{"selected",true},{"originalFileExists",true},{"layers",entry["levels"]},{"playable",true},{"mode","original-exploration"},{"nativeObjects",game.objects.size()},{"nativeDecorations",game.decorations.size()}});
  game=Game::create();scene=Scene::Maps;
 }
 key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Scenario,"Map list Escape opened the wrong screen");render();check(buttons_[10].enabled,"Scenario start remained disabled after importing maps");
 auto original=mapLibrary_["entries"][selectedMap_]["scenarioFile"];mapLibrary_["entries"][selectedMap_]["scenarioFile"]="assets/demo/scenarios/missing.json";key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Scenario&&game.toJson()==state&&!toast_.empty(),"Failed map load replaced the current game");mapLibrary_["entries"][selectedMap_]["scenarioFile"]=original;shot("community-start-enabled");
 render();tap({120,562});selectMap(0);render();tap({630,560});check(scene==Scene::Scenario,"Practice map could not be selected again");render();key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Adventure&&game.town.faction==8,"Practice map could not start after browsing community maps");
 scene=Scene::Maps;selectMap(0);render();SDL_Event wheel{};wheel.type=SDL_MOUSEWHEEL;wheel.wheel.y=-1;handleEvent(wheel);check(mapPage_==1,"Map list did not scroll to next page");key(SDLK_PAGEDOWN,KMOD_NONE);check(selectedMap_==6&&mapPage_==1,"Map page shortcut did not move selection");
 game=Game::create();auto beforeDialog=game.toJson();visitPopup_=AdventureVisit{ObjectType::Site,"长地图事件","第一段：请阅读地图线索。\n第二段：英雄需要先完成任务。\n第三段：原图的事件文字应当能够完整翻页阅读。\n第四段：抵达地下层后，可以通过通道返回地上。\n第五段：确认关闭窗口后，再继续探索和战斗。"};scene=Scene::Visit;focus_=-1;render();check(buttons_.size()==3&&buttons_[1].enabled,"Long map events did not offer a next page");shot("long-event-first");tap({548,385});render();check(visitPage_==1,"Map event next-page button did not advance");shot("long-event-next");key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Adventure&&!visitPopup_&&game.toJson()==beforeDialog,"Enter did not acknowledge a paginated map event cleanly");
 game=Game::create();game.objects.clear();for(auto&t:game.tiles){t.seen=true;t.decor=Decor::None;t.terrain=Terrain::Grass;t.road=t.river=false;}game.hero.pos={18,14};game.hero.onBoat=true;scene=Scene::Adventure;camX_=18.5f;camY_=14.5f;zoom_=1;heroDirection_={1,0};route_.clear();destination_.reset();mouseX_=mouseY_=-1;shot("boat-flag-blue");
 auto center=mapScreen(game.hero.pos);SDL_Rect area{int((center.x-48)*pixelScale_),int((center.y-48)*pixelScale_),int(96*pixelScale_),int(64*pixelScale_)};std::vector<Uint8>pixels(size_t(area.w)*area.h*4);check(SDL_SetRenderTarget(renderer_,canvas_.texture)==0,"Boat color canvas unavailable");bool read=SDL_RenderReadPixels(renderer_,&area,SDL_PIXELFORMAT_RGBA32,pixels.data(),area.w*4)==0;SDL_SetRenderTarget(renderer_,nullptr);check(read,"Boat flag pixels unavailable");int bluePixels=0;for(size_t i=0;i<pixels.size();i+=4)bluePixels+=pixels[i]<24&&pixels[i+1]<24&&pixels[i+2]>120;check(bluePixels>20,"Embarked hero flag did not match friendly blue ownership");
 std::ofstream(directory/"scenario-selection-audit.json")<<nlohmann::json{{"townSelections",selections},{"wholeCardAndCaption",true},{"dragOffCancels",true},{"numberShortcuts",true},{"downloadedMaps",maps},{"originalMapsStartWithoutSubstitution",true},{"failedLoadKeepsGame",true},{"practiceMapSelectableAfterBrowsing",true},{"pagingAndLayers",true},{"boatFlagBlue",true},{"boatFlagBluePixels",bluePixels}}.dump(2)<<'\n';
 reset();SDL_SetWindowSize(window_,oldW,oldH);settings_=previous;scene=Scene::Town;fixedAnimationTime_=0;
}

}
