#include "h3/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace h3 {
namespace {
constexpr SDL_Color GoldColor{236,213,130,255},Muted{189,171,135,255};
constexpr int RecruitX=143,RecruitY=17,BuildX=202,BuildY=26;
constexpr std::array<int,7> ResourceOrder{Wood,Mercury,Ore,Sulfur,Crystal,Gems,Gold};
constexpr std::array<const char*,9> CreatureBackgrounds{"crbkgcas.pcx","crbkgram.pcx","crbkgtow.pcx","crbkginf.pcx","crbkgnec.pcx","crbkgdun.pcx","crbkgstr.pcx","crbkgfor.pcx","crbkgele.pcx"};
bool contains(SDL_Rect r,Point p){return p.x>=r.x&&p.y>=r.y&&p.x<r.x+r.w&&p.y<r.y+r.h;}
SDL_Rect armyCell(bool garrison,int slot){return {303+62*slot,garrison?386:484,58,64};}
std::string buildMessage(BuildState state){switch(state){
 case BuildState::Ready:return "可以建造";case BuildState::Owned:return "已经建造";
 case BuildState::AlreadyBuiltToday:return "今天已经建造过建筑";case BuildState::MissingRequirements:return "尚未满足前置建筑要求";
 case BuildState::NoResources:return "资源不足";case BuildState::Unsupported:return "该建筑的功能仍在实现";
 default:return "当前无法建设";
}}
}
void Desktop::centerText(const std::string&s,SDL_Rect r,int size,SDL_Color color){
  auto*face=font(size);
 int width=0,height=0;TTF_SizeUTF8(face,s.c_str(),&width,&height);
 text(s,r.x+(r.w-int(std::round(width/pixelScale_)))/2,r.y+(r.h-int(std::round(height/pixelScale_)))/2,size,color);
}
void Desktop::rightText(const std::string&s,SDL_Rect r,int size,SDL_Color color){
  auto*face=font(size);
 int width=0,height=0;TTF_SizeUTF8(face,s.c_str(),&width,&height);
 text(s,r.x+r.w-int(std::round(width/pixelScale_)),r.y+(r.h-int(std::round(height/pixelScale_)))/2,size,color);
}
const PixelMask& Desktop::pixelMask(const std::string&file){
 if(auto found=masks_.find(file);found!=masks_.end())return found->second;
 std::string error;auto mask=readPixelMask(root_/file,error);
 if(!mask)throw std::runtime_error(file+": "+error);
 return masks_.emplace(file,std::move(*mask)).first->second;
}
bool Desktop::layerVisible(const TownLayer&layer)const{
 return std::all_of(layer.needs.begin(),layer.needs.end(),[this](int id){return game.town.built[id];})&&
        std::none_of(layer.excludes.begin(),layer.excludes.end(),[this](int id){return game.town.built[id];});
}
int Desktop::townLayerAt(Point point){
 if(point.x<0||point.y<0||point.x>=800||point.y>=374)return -1;
 const auto&layers=game.townDef().layers;
 for(int index=int(layers.size())-1;index>=0;--index){const auto&layer=layers[index];
  if(layerVisible(layer)&&!layer.mask.empty()&&pixelMask(layer.mask).contains(point.x-layer.pos.x,point.y-layer.pos.y))return index;
 }
 return -1;
}
void Desktop::townScene(){
 redrawnBuildingsDrawn_=0;redrawnAnimatedBuildingsDrawn_=0;
 if(!redrawnTown())art("original/"+Factions[game.town.faction].background,{0,0,800,374},{},false);
 SDL_Rect clip{0,0,800,374};SDL_RenderSetClipRect(renderer_,&clip);
 const auto&layers=game.townDef().layers;int hovered=scene==Scene::Town&&settings_.townOutlines?townLayerAt({mouseX_,mouseY_}):-1;
 for(int index=0;index<int(layers.size());index++){const auto&layer=layers[index];if(!layerVisible(layer))continue;
  if(!redrawnTownBuilding(layer)&&!redrawnAnimatedTownBuilding(layer)&&!townAnimation(layer)&&!layer.art.empty()){auto&im=image("original/"+layer.art);art("original/"+layer.art,{layer.pos.x,layer.pos.y,im.w,im.h});}
  if(index==hovered&&!layer.outline.empty()){const auto&mask=pixelMask(layer.mask);art(layer.outline,{layer.pos.x,layer.pos.y,mask.width,mask.height},{},false);}
 }
 SDL_RenderSetClipRect(renderer_,nullptr);
}
void Desktop::townFooter(bool hall){
 originalImage(hall?"kresbar.pcx":"aresbar.pcx",{3,578,hall?738:794,22});
 int cell=hall?79:85;for(int i=0;i<7;i++)text(std::to_string(game.resources[ResourceOrder[i]]),30+cell*i,582,10);
 int month=(game.day-1)/28+1,week=((game.day-1)%28)/7+1,day=(game.day-1)%7+1;
 centerText("月 "+std::to_string(month)+"，周 "+std::to_string(week)+"，日 "+std::to_string(day),{hall?558:603,578,hall?181:192,22},11);
}
std::optional<std::pair<bool,int>> Desktop::townArmyAt(Point p)const{
 for(bool garrison:{true,false})if(garrison||game.visiting())for(int slot=0;slot<7;slot++)if(contains(armyCell(garrison,slot),p))return std::pair{garrison,slot};
 return {};
}
void Desktop::townArmyRow(bool garrison){
 if(!garrison&&!game.visiting())return;
 const Army&army=garrison?game.town.garrison:game.hero.army;
 for(int slot=0;slot<7;slot++){auto r=armyCell(garrison,slot);const auto&stack=army[slot];
  if(!stack.empty()){portrait(stack.unit,r);rightText(std::to_string(stack.count),{r.x+1,r.y+48,r.w-3,16},12);}
  if(sourceSlot_==slot&&sourceGarrison_==garrison)outline({r.x-1,r.y-1,r.w+2,r.h+2},{255,241,125,255});
  buttons_.push_back({r,[this,garrison,slot]{
   if(sourceSlot_==slot&&sourceGarrison_==garrison){armyReturn_=Scene::Town;scene=Scene::Army;focus_=-1;return;}
   selectStack(garrison,slot,(pointerModifiers_.value_or(SDL_GetModState())&KMOD_SHIFT)!=0);
  },true,{},(stack.empty()?std::string("空部队栏"):Units[stack.unit].name+" × "+std::to_string(stack.count))+"；点击选择或交换，拖动转移，Shift 点击拆分"});
 }
}
void Desktop::townDrag(Point p){
 if(!armyDrag_||!armyDrag_->moving)return;
 const auto&stack=(armyDrag_->garrison?game.town.garrison:game.hero.army)[armyDrag_->slot];
 if(!stack.empty()){portrait(stack.unit,{p.x-29,p.y-32,58,64});outline({p.x-29,p.y-32,58,64},GoldColor);}
}
void Desktop::town(){
 std::string townName=Factions[game.town.faction].name+"主城";if(auto*o=game.objectById(game.activeTownId))townName=o->name;
 townScene();originalImage("townscrn.pcx",{0,374,800,226});
 // Each building receives only the nontransparent pixels of its original TZ
 // mask (or an explicitly recorded sprite-derived fallback), in painting order.
 for(const auto&layer:game.townDef().layers)if(layerVisible(layer)&&!layer.mask.empty()&&!layer.needs.empty()){
  const auto&mask=pixelMask(layer.mask);Point origin=layer.pos;std::string file=layer.mask;int id=layer.needs.front();
  buttons_.push_back({{origin.x,origin.y,mask.width,std::min(mask.height,374-origin.y)},[this,id]{townBuilding(id);},true,
    [this,file,origin](Point p){return p.y<374&&pixelMask(file).contains(p.x-origin.x,p.y-origin.y);},game.townDef().buildings[id].name+"：点击访问建筑"});
 }
 int fortLevel=game.town.built[Castle]?2:game.town.built[Citadel]?1:game.town.built[Fort]?0:3;
 int hallLevel=game.town.built[Capitol]?3:game.town.built[CityHall]?2:game.town.built[TownHall]?1:0;
 frame("itpt.def",0,game.town.faction*2+(fortLevel==3?18:0),{15,387,58,64});
 centerText(townName,{81,387,144,20},14,GoldColor);
 frame("itmcl.def",0,fortLevel,{82,410,38,38});frame("itmtl.def",0,hallLevel,{125,410,38,38});
 centerText(std::to_string(game.income()),{172,433,56,19},12);
 buttons_.push_back({{82,410,38,38},[this]{scene=Scene::Fort;focus_=-1;},true,{},"打开堡垒，查看各级兵种并招募部队"});
 buttons_.push_back({{125,410,38,38},[this]{scene=Scene::Build;focus_=-1;},true,{},"打开大厅：查看建筑条件、费用并建设；每天可建设一次"});
 auto units=game.recruitableUnits();for(int tier=0;tier<7;tier++)if(units[tier]>=0){int unit=units[tier];
  SDL_Rect cell{14+54*(tier<4?tier:tier-4),tier<4?458:507,48,48};
  frame("cprsmall.def",0,Units[unit].portraitFrame,{cell.x+8,cell.y+1,32,32});
  centerText("+"+std::to_string(game.weeklyGrowth(tier)),{cell.x,cell.y+33,48,14},11);
  buttons_.push_back({cell,[this,unit]{openRecruit(unit,Scene::Town);},true,{},"招募"+Units[unit].name+"：查看当前库存、价格与数量"});
 }
 frame("crest58.def",0,1,{241,386,58,64});
 if(game.visiting())originalImage("hpl128qc.pcx",{241,484,58,64});
 townArmyRow(true);townArmyRow(false);
 if(game.constructionState(GrailBuilding)==BuildState::Ready)button({80,388,146,22},"建造圣杯",[this]{say(game.build(GrailBuilding));persist();});
 frame("itpa.def",0,2+game.town.faction*2+(fortLevel==3?18:0),{745,441,48,32});
 buttons_.push_back({{744,543,54,29},[this]{scene=Scene::Adventure;sourceSlot_=-1;},true,{},"离开城镇，返回冒险地图（Esc）"});
 std::string status=toastUntil_>SDL_GetTicks64()?toast_:townName;
 int hovered=townLayerAt({mouseX_,mouseY_});if(hovered>=0&&!game.townDef().layers[hovered].needs.empty())status=game.townDef().buildings[game.townDef().layers[hovered].needs.front()].name;
 centerText(status,{8,554,733,20},11);townFooter();townDrag({mouseX_,mouseY_});
}
void Desktop::townBuilding(int id){
 if(id==Shipyard&&game.imported()){say(game.buyBoat());persist();return;}
 if(id==Tavern&&game.imported()){scene=Scene::Tavern;focus_=-1;return;}
 focus_=-1;
 if(id==Blacksmith||(id>=Special1&&id<=Special4)){game.refreshTownServices();if(!game.townOffers(id).empty()){townServiceBuilding_=id;locationPage_=0;scene=Scene::TownService;return;}}
 if(id==VillageHall||id==TownHall||id==CityHall||id==Capitol){scene=Scene::Build;return;}
 if(id==Fort||id==Citadel||id==Castle){scene=Scene::Fort;return;}
 if(id==Marketplace||id==ResourceSilo){openMarket();return;}
 if(id==MageGuild||id==MageGuild2||id==MageGuild3||id==MageGuild4||id==MageGuild5){scene=Scene::MageGuild;game.learnTownSpells();persist();return;}
 if(id>=Guardhouse&&id<=UpPortal){int tier=(id<=Portal?id-Guardhouse:id-UpGuardhouse);auto units=game.recruitableUnits();if(units[tier]>=0)openRecruit(units[tier],Scene::Town);return;}
 if(id==Special2&&game.town.faction==8){if(game.visiting())scene=Scene::University;else say("英雄需要进入城镇。");return;}
 buildingPopup(id,Scene::Town);
}
void Desktop::buildingPopup(int id,Scene back){selectedBuilding_=id;buildingReturn_=back;focus_=-1;scene=Scene::BuildConfirm;}
void Desktop::build(){
 const auto&town=game.townDef();bool original=originalImage(town.hallBackground,{0,0,800,600});
 if(!original){
  // Six faction hall backgrounds are absent from the demo. Keep that gap in
  // town-interface.json, using only a neutral wood strip as a temporary skin.
  originalImage("tpthbkcs.pcx",{0,0,800,600});
  for(int x=8;x<792;x+=16)originalImage("tpthbkcs.pcx",{x,23,std::min(16,792-x),529},SDL_Rect{777,23,std::min(16,792-x),529});
 }
 centerText("大厅",{0,1,800,25},18,GoldColor);
 for(int row=0;row<int(town.hallSlots.size());row++){
  int columns=int(town.hallSlots[row].size()),start=400-(columns-1)*97-76;
  for(int column=0;column<columns;column++){int id=game.hallBuilding(row,column);if(id<0)continue;
   int x=start+column*194,y=35+row*104;auto state=game.constructionState(id);bool owned=state==BuildState::Owned;
   frame(town.hallIcons,0,game.hallIcon(id),{x+1,y+1,150,70});
   int bar=owned?0:state==BuildState::Ready?1:state==BuildState::AlreadyBuiltToday||state==BuildState::Unavailable||state==BuildState::Unsupported?3:2;
   frame("tpthbar.def",0,bar,{x,y+73,152,18});
   centerText(town.buildings[id].name,{x+1,y+73,150,18},11);
   if(owned)frame("tpthchk.def",0,0,{x+133,y+54,16,16});
   else if(bar==2)frame("tpthchk.def",0,1,{x+133,y+54,16,16});
   buttons_.push_back({{x,y,152,91},[this,id]{buildingPopup(id,Scene::Build);},true,{},town.buildings[id].name+"：查看前置条件、费用与建造说明"});
  }
 }
 std::string status=game.town.builtDay==game.day?"今天已经建造过建筑。":"每天可以建造一座建筑。";
 for(const auto&b:buttons_)if(b.contains({mouseX_,mouseY_})){int row=(b.rect.y-35)/104;int column=(b.rect.x-(400-(int(town.hallSlots[row].size())-1)*97-76))/194;int id=game.hallBuilding(row,column);status=town.buildings[id].name+"："+buildMessage(game.constructionState(id));break;}
 centerText(status,{8,555,733,20},11);townFooter(true);
 originalButton({746,562,52,36},"hsbtns.def",0,[this]{scene=Scene::Town;focus_=-1;});
}
void Desktop::buildConfirm(){
 int savedX=mouseX_,savedY=mouseY_;mouseX_=mouseY_=-1000;
 if(buildingReturn_==Scene::Build)build();else town();mouseX_=savedX;mouseY_=savedY;buttons_.clear();
 if(selectedBuilding_<0||selectedBuilding_>=BuildingCount){scene=buildingReturn_;return;}
 const auto&building=game.townDef().buildings[selectedBuilding_];auto state=game.constructionState(selectedBuilding_);
 originalImage("tpubuild.pcx",{BuildX,BuildY,395,521});
 centerText(building.name,{BuildX+12,BuildY+9,371,32},19,GoldColor);
 frame(game.townDef().hallIcons,0,game.hallIcon(selectedBuilding_),{BuildX+123,BuildY+49,150,70});
 text(building.description,BuildX+41,BuildY+144,13,GoldColor,312);
 std::string requirements="前置建筑：";
 bool first=true;for(const auto&option:building.requirements){if(!first)requirements+=" 或 ";first=false;bool item=false;for(int id:option){if(item)requirements+="、";item=true;requirements+=game.townDef().buildings[id].name;}}
 if(building.requirements.empty()||(building.requirements.size()==1&&building.requirements[0].empty()))requirements+="无";
 text(requirements,BuildX+41,BuildY+225,12,Muted,312);
 if(state!=BuildState::Owned){std::vector<int> resources;for(int r:ResourceOrder)if(building.cost[r]>0)resources.push_back(r);
  for(int i=0;i<int(resources.size());i++){int r=resources[i],columns=std::min(4,int(resources.size())),col=i%4,row=i/4;
   int x=BuildX+197-columns*39+col*78;marketIcon(r,{x+23,BuildY+305+row*57,32,32});
   centerText(std::to_string(building.cost[r]),{x,BuildY+338+row*57,78,20},12,building.cost[r]>game.resources[r]?SDL_Color{255,120,100,255}:GoldColor);
  }
 }
 centerText(buildMessage(state),{BuildX+14,BuildY+492,367,23},12);
 // Default Return closes a read-only information window; a ready building
 // requires the affirmative control, and build() rechecks every rule at commit.
 if(state==BuildState::Owned){originalButton({BuildX+165,BuildY+447,64,32},"iokay.def",0,[this]{scene=buildingReturn_;focus_=-1;});}
 else{
  originalButton({BuildX+77,BuildY+447,64,32},"iokay.def",0,[this]{int id=selectedBuilding_;say(game.build(id));persist();if(game.town.built[id]){scene=Scene::Town;sourceSlot_=-1;focus_=-1;}},state==BuildState::Ready,2,"建造"+building.name+"："+cost(building.cost)+"；"+buildMessage(state));
  originalButton({BuildX+254,BuildY+447,64,30},"icancel.def",0,[this]{scene=buildingReturn_;focus_=-1;});
 }
}
void Desktop::unitPreview(int unit,SDL_Rect destination,bool fort){
 if(unit<0||unit>=UnitCount)return;
 int faction=Units[unit].faction;
 const char*background=faction>=0?CreatureBackgrounds[faction]:"crbkgneu.pcx";
 if(fort){constexpr std::array<const char*,9> fortBackgrounds{"tpcascas.pcx","tpcasram.pcx","tpcastow.pcx","tpcasinf.pcx","tpcasnec.pcx","tpcasdun.pcx","tpcasstr.pcx","tpcasfor.pcx","tpcasele.pcx"};
  const char*original=faction>=0?fortBackgrounds[faction]:"tpcasneu.pcx";
  if(demo_.contains("pcx")&&demo_["pcx"].contains(original))background=original;
 }
 originalImage(background,destination);
 bool clipped=SDL_RenderIsClipEnabled(renderer_);SDL_Rect previous;SDL_RenderGetClipRect(renderer_,&previous);
 SDL_Rect clip=destination;if(clipped)SDL_IntersectRect(&destination,&previous,&clip);SDL_RenderSetClipRect(renderer_,&clip);
 const auto&name=Units[unit].animation;
 if(frameCount(name,2)){
  const auto&f=demo_["def"][name]["groups"]["2"][0];int w=f["canvas"][0],h=f["canvas"][1];
  // Combat-canvas foot anchor remains provisional until executable comparison.
  frame(name,2,int(animationTime_/100),{destination.x+destination.w/2-196,destination.y+destination.h-10-268,w,h});
 }else portrait(unit,{destination.x+(destination.w-58)/2,destination.y+(destination.h-64)/2,58,64});
 SDL_RenderSetClipRect(renderer_,clipped?&previous:nullptr);
}
void Desktop::fort(){
 originalImage("tpcastl7.pcx",{0,0,800,600});centerText(game.town.built[Castle]?"城堡":game.town.built[Citadel]?"大本营":"堡垒",{0,0,800,24},18,GoldColor);
 auto units=game.recruitableUnits();
 for(int tier=0;tier<7;tier++){
  int x=tier==6?205:tier%2*394+8,y=tier==6?421:23+tier/2*133;
  int unit=units[tier]>=0?units[tier]:game.town.faction*14+tier*2;
  int dwelling=(unit%2?UpGuardhouse:Guardhouse)+tier;const auto&u=Units[unit];
  frame(game.townDef().hallIcons,0,game.hallIcon(dwelling),{x+7,y+21,150,70});
  centerText(u.name,{x+7,y+1,150,18},12);
  centerText(game.townDef().buildings[dwelling].name,{x+7,y+91,150,18},11);
  if(units[tier]>=0)centerText("可招募："+std::to_string(game.town.pool[tier]),{x+7,y+110,150,17},11);
  unitPreview(unit,{x+161,y+1,100,124},true);
  const std::string values[]={std::to_string(u.attack),std::to_string(u.defense),std::to_string(u.minDamage)+"–"+std::to_string(u.maxDamage),std::to_string(u.hp),std::to_string(u.speed),std::to_string(game.weeklyGrowth(tier))};
  const char* labels[]={"攻击","防御","伤害","生命","速度","增长"};
  for(int stat=0;stat<6;stat++){originalImage("tpcainfo.pcx",{x+266,y+2+20*stat,20,19},SDL_Rect{1,stat*20,21,19});text(labels[stat],x+291,y+5+20*stat,11);centerText(values[stat],{x+337,y+2+20*stat,47,20},11);}
  buttons_.push_back({{x+7,y+1,254,124},[this,unit]{openRecruit(unit,Scene::Fort);},units[tier]>=0,{},"招募"+Units[unit].name+"：选择数量并购买"});
 }
 centerText("选择生物进行招募。",{8,555,733,20},11);townFooter(true);
 originalButton({746,562,52,36},"hsbtns.def",0,[this]{scene=Scene::Town;focus_=-1;});
}
int Desktop::recruitLimit()const{
 if(game.battle||game.report||game.result||!game.hero.levelChoices.empty())return 0;
 if(!recruitGarrison_&&!game.visiting())return 0;
 const auto&army=recruitGarrison_?game.town.garrison:game.hero.army;
 for(const auto&stack:army)if(!stack.empty()&&stack.unit==recruitUnit_)return std::min(game.recruitMax(recruitUnit_),MaxArmyStack-stack.count);
 if(std::none_of(army.begin(),army.end(),[](const auto&s){return s.empty();}))return 0;
 return game.recruitMax(recruitUnit_);
}
void Desktop::openRecruit(int unit,Scene back){
 if(unit<0||unit>=UnitCount||baseTier(unit)<0)return;
 recruitUnit_=unit;recruitGarrison_=!game.visiting();recruitReturn_=back;recruitAmount_=std::min(1,recruitLimit());recruitDragging_=false;focus_=-1;scene=Scene::Recruit;
}
void Desktop::purchaseRecruit(){
 if(recruitAmount_<=0||recruitAmount_>recruitLimit())return;
 int before=game.town.pool[baseTier(recruitUnit_)];say(game.recruit(recruitUnit_,recruitAmount_,recruitGarrison_));persist();
 if(game.town.pool[baseTier(recruitUnit_)]<before){scene=recruitReturn_;focus_=-1;}
 recruitDragging_=false;
}
SDL_Rect Desktop::recruitTrack()const{return {RecruitX+189,RecruitY+279,107,16};}
void Desktop::recruitAmount(int mouseX){auto track=recruitTrack();int span=track.w-16;int position=std::clamp(mouseX-track.x-8,0,span);recruitAmount_=int((int64_t(position)*recruitLimit()+span/2)/span);}
void Desktop::recruit(){
 int savedX=mouseX_,savedY=mouseY_;mouseX_=mouseY_=-1000;
 if(recruitReturn_==Scene::Fort)fort();else town();mouseX_=savedX;mouseY_=savedY;buttons_.clear();
 originalImage("tprcrt.pcx",{RecruitX,RecruitY,485,395});
 if(recruitUnit_<0||recruitUnit_>=UnitCount||baseTier(recruitUnit_)<0){scene=recruitReturn_;return;}
 int tier=baseTier(recruitUnit_),limit=recruitLimit();recruitAmount_=std::clamp(recruitAmount_,0,limit);
 centerText("招募"+Units[recruitUnit_].name,{RecruitX+15,RecruitY+15,455,34},20,GoldColor);
 unitPreview(recruitUnit_,{RecruitX+193,RecruitY+65,100,130});
 centerText("单价",{RecruitX+66,RecruitY+222,99,18},12);centerText("可招募",{RecruitX+173,RecruitY+222,67,18},12);
 centerText("数量",{RecruitX+247,RecruitY+222,67,18},12);centerText("总价",{RecruitX+324,RecruitY+222,99,18},12);
 centerText(std::to_string(std::max(0,game.town.pool[tier]-recruitAmount_)),{RecruitX+173,RecruitY+240,67,22},12);
 centerText(std::to_string(recruitAmount_),{RecruitX+247,RecruitY+240,67,22},12);
 auto single=unitCost(recruitUnit_),total=unitCost(recruitUnit_,recruitAmount_);std::vector<int> resources;for(int r:ResourceOrder)if(single[r]>0)resources.push_back(r);
 for(int i=0;i<int(resources.size());i++)for(int side=0;side<2;side++){
  int r=resources[i],x=RecruitX+(side?324:66),w=99/int(resources.size());
  marketIcon(r,{x+i*w+(w-32)/2,RecruitY+242,32,32});centerText(std::to_string((side?total:single)[r]),{x+i*w,RecruitY+276,w,18},11);
 }
 auto track=recruitTrack();box(track,{18,17,14,255});int span=track.w-16,knob=limit?int(int64_t(recruitAmount_)*span/limit):0;
 frame("igpcrdiv.def",0,4,{track.x+knob,track.y,16,16});
 buttons_.push_back({track,[this]{recruitAmount(mouseX_);},limit>0,{},"拖动选择招募数量；← / → 微调，End 选最大数量"});
 originalButton({track.x-16,track.y,16,16},"igpcrdiv.def",0,[this]{recruitAmount_=std::max(0,recruitAmount_-1);},recruitAmount_>0);
 originalButton({track.x+track.w,track.y,16,16},"igpcrdiv.def",2,[this,limit]{recruitAmount_=std::min(limit,recruitAmount_+1);},recruitAmount_<limit);
 originalButton({RecruitX+134,RecruitY+312,64,32},"ircbtns.def",0,[this,limit]{recruitAmount_=limit;},limit>0,2);
 originalButton({RecruitX+211,RecruitY+313,64,30},"ibuy30.def",0,[this]{purchaseRecruit();},recruitAmount_>0,2);
 originalButton({RecruitX+291,RecruitY+313,64,30},"icancel.def",0,[this]{scene=recruitReturn_;recruitDragging_=false;focus_=-1;});
 // Supplemental controls retain explicit hero/garrison allocation while the
 // original stationed-hero model is still absent. G changes recipient, U tier.
 std::string status=toastUntil_>SDL_GetTicks64()?toast_:std::string("接收：")+(recruitGarrison_?"驻军":"英雄")+"  G 切换"+(game.town.built[UpGuardhouse+tier]?"  U 切换兵种":"");
 centerText(status,{RecruitX+14,RecruitY+372,457,20},10);
 buttons_.push_back({{RecruitX+14,RecruitY+372,457,20},[this]{if(game.visiting()){recruitGarrison_=!recruitGarrison_;recruitAmount_=std::min(recruitAmount_,recruitLimit());}},game.visiting(),{},"切换招募目标：到访英雄 / 城镇驻军；英雄必须位于城镇"});
}
void Desktop::townRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/(std::string(name)+".png")),"Town interface screenshot failed");};
 auto mouse=[this](Uint32 type,int x,int y){SDL_Event e{};e.type=type;
  if(type==SDL_MOUSEMOTION){e.motion.x=x;e.motion.y=y;}else{e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;}handleEvent(e);
 };
 auto drag=[&](Point from,Point to,bool shift=false){render();mouse(SDL_MOUSEBUTTONDOWN,from.x,from.y);mouse(SDL_MOUSEMOTION,to.x,to.y);render();SDL_SetModState(shift?KMOD_SHIFT:KMOD_NONE);mouse(SDL_MOUSEBUTTONUP,to.x,to.y);SDL_SetModState(KMOD_NONE);};
 game=Game::create();scene=Scene::Town;sourceSlot_=-1;focus_=-1;render();
 // A point inside a building's rectangular canvas, but outside all active
 // mask pixels, must not open any building.
 bool transparent=false;for(const auto&layer:game.townDef().layers)if(layerVisible(layer)&&!layer.mask.empty()){
  const auto&mask=pixelMask(layer.mask);for(int y=0;y<mask.height&&!transparent;y++)for(int x=0;x<mask.width&&!transparent;x++){
   Point p{layer.pos.x+x,layer.pos.y+y};if(p.x>=0&&p.x<800&&p.y>=0&&p.y<374&&!mask.contains(x,y)&&townLayerAt(p)<0){
    auto before=game.toJson();mouse(SDL_MOUSEBUTTONDOWN,p.x,p.y);mouse(SDL_MOUSEBUTTONUP,p.x,p.y);check(scene==Scene::Town&&game.toJson()==before,"Transparent building pixel accepted a click");transparent=true;
   }
  }
  if(transparent)break;
 }
 check(transparent,"Town mask has no transparent test point");
 click({145,428},false);check(scene==Scene::Build,"Original hall icon did not open construction");shot("hall-ready");
 int marketColumn=-1;for(int col=0;col<int(game.townDef().hallSlots[1].size());col++)if(game.hallBuilding(1,col)==Marketplace)marketColumn=col;
 check(marketColumn>=0,"Castle hall market slot missing");int marketX=400-(int(game.townDef().hallSlots[1].size())-1)*97-76+marketColumn*194;
 auto before=game.toJson();click({marketX+70,170},false);check(scene==Scene::BuildConfirm&&selectedBuilding_==Marketplace,"Hall card did not open market confirmation");shot("build-confirm-ready");
 key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Build&&game.toJson()==before,"Cancelled construction mutated state");render();click({marketX+70,170},false);render();
 click({BuildX+100,BuildY+462},false);check(scene==Scene::Town&&game.town.built[Marketplace]&&game.resources[Gold]==9500&&game.resources[Wood]==15,"Confirmed construction charged an incorrect cost");
 key(SDLK_h,KMOD_NONE);shot("hall-built-today");check(game.hallBuilding(1,marketColumn)==ResourceSilo,"Hall did not advance to the resource silo");
 render();click({marketX+70,170},false);check(selectedBuilding_==ResourceSilo,"Hall upgrade chain selected the owned market");before=game.toJson();render();click({BuildX+100,BuildY+462},false);check(game.toJson()==before,"Disabled construction confirmation changed state");key(SDLK_ESCAPE,KMOD_NONE);key(SDLK_ESCAPE,KMOD_NONE);
 for(int faction=0;faction<FactionCount;faction++){
  game=Game::create(1999,faction);scene=Scene::Build;shot(("hall-"+std::to_string(faction)).c_str());
  scene=Scene::Fort;shot(("fort-"+std::to_string(faction)).c_str());
 }
 game=Game::create();scene=Scene::Town;sourceSlot_=-1;
 drag({320,500},{320,405});check(game.hero.army[0].empty()&&game.town.garrison[0].count==28&&sourceSlot_<0,"Town drag did not transfer the whole stack");shot("town-drag-transferred");
 before=game.toJson();drag({320,405},{320,500},true);check(scene==Scene::Split&&splitReturn_==Scene::Town&&splitAmount_==14,"Shift town drag did not open a half-stack split");
 key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Town&&game.toJson()==before,"Cancelling town split moved units");
 drag({320,405},{320,500},true);shot("town-split");click({200,440},false);
 check(scene==Scene::Town&&game.town.garrison[0].count==14&&game.hero.army[0].count==14,"Town split did not return to the original scene");
 game.hero.pos={8,18};drag({320,405},{382,405});check(game.town.garrison[0].empty()&&game.town.garrison[1].count==14,"Remote garrison rearrangement was blocked");
 before=game.toJson();drag({382,405},{320,500});check(game.toJson()==before,"Remote garrison drag entered the absent hero army");shot("town-remote-garrison");
 mouse(SDL_MOUSEBUTTONDOWN,382,405);mouse(SDL_MOUSEMOTION,430,420);SDL_Event lost{};lost.type=SDL_WINDOWEVENT;lost.window.event=SDL_WINDOWEVENT_FOCUS_LOST;handleEvent(lost);
 check(!armyDrag_&&!mouseLeftDown_&&pressedButton_<0,"Focus loss retained a town army drag");mouse(SDL_MOUSEBUTTONUP,444,405);check(game.toJson()==before,"Focus loss allowed a stale army drop");
 game=Game::create();game.hero.army={Stack{Pikeman,1}};scene=Scene::Town;before=game.toJson();drag({320,500},{320,405});
 check(game.toJson()==before,"Town drag removed the hero's last unit");
 game=Game::create();scene=Scene::Town;key(SDLK_r,KMOD_NONE);render();click({70,80},false);check(scene==Scene::Recruit&&recruitReturn_==Scene::Fort,"Fort recruitment navigation failed");
 auto track=recruitTrack();before=game.toJson();mouse(SDL_MOUSEBUTTONDOWN,track.x+8,track.y+8);check(recruitDragging_&&recruitAmount_==0,"Recruit slider did not capture mouse down");
 mouse(SDL_MOUSEMOTION,track.x+track.w-8,track.y+8);check(recruitAmount_==14,"Recruit slider drag maximum failed");
 mouse(SDL_MOUSEBUTTONUP,track.x+1000,track.y+8);check(!recruitDragging_&&recruitAmount_==14&&game.toJson()==before,"Recruit slider committed a purchase");shot("recruit-slider-max");
 key(SDLK_HOME,KMOD_NONE);key(SDLK_RIGHT,KMOD_NONE);check(recruitAmount_==1,"Recruit keyboard quantity failed");key(SDLK_g,KMOD_NONE);check(recruitGarrison_&&game.toJson()==before,"Recruit recipient selection mutated game");
 render();mouse(SDL_MOUSEBUTTONDOWN,RecruitX+150,RecruitY+326);shot("recruit-max-pressed");mouse(SDL_MOUSEBUTTONUP,RecruitX+150,RecruitY+326);check(recruitAmount_==14,"Recruit maximum capture failed");
 render();mouse(SDL_MOUSEBUTTONDOWN,RecruitX+225,RecruitY+326);mouse(SDL_MOUSEBUTTONUP,RecruitX+305,RecruitY+326);check(game.toJson()==before&&scene==Scene::Recruit,"Releasing Cancel committed recruitment");
 mouse(SDL_MOUSEBUTTONDOWN,track.x+8,track.y+8);handleEvent(lost);check(!recruitDragging_&&!mouseLeftDown_,"Focus loss retained recruit slider capture");
 key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Fort&&game.toJson()==before,"Recruit cancel changed stock or return scene");
 scene=Scene::Town;openRecruit(Pikeman,Scene::Town);game.town.built[UpGuardhouse]=true;key(SDLK_u,KMOD_NONE);check(recruitUnit_==Halberdier,"Recruit upgrade tier keyboard selection failed");key(SDLK_u,KMOD_NONE);check(recruitUnit_==Pikeman,"Recruit base tier keyboard selection failed");
 game.town.garrison={Stack{Archer,1},Stack{Griffin,1},Stack{Swordsman,1},Stack{Monk,1},Stack{Cavalier,1},Stack{Angel,1},Stack{Halberdier,1}};recruitGarrison_=true;
 before=game.toJson();shot("recruit-no-room");check(recruitLimit()==0&&recruitAmount_==0,"Full garrison allowed an incompatible recruit stack");click({385,345},false);check(game.toJson()==before,"Disabled recruit button charged resources");
 game=Game::create();game.hero.army[0].count=MaxArmyStack-1;openRecruit(Pikeman,Scene::Town);key(SDLK_END,KMOD_NONE);check(recruitAmount_==1,"Recruit UI ignored remaining stack capacity");key(SDLK_RETURN,KMOD_NONE);check(game.hero.army[0].count==MaxArmyStack&&game.validate(),"Recruit UI exceeded native save capacity");
 game=Game::create();game.town.built[Portal]=true;game.town.pool[6]=4;game.resources[Gold]=100000;game.resources[Gems]=2;
 openRecruit(Angel,Scene::Town);key(SDLK_END,KMOD_NONE);check(recruitAmount_==2,"Rare resource recruitment cap failed");shot("recruit-rare-cost");int day=game.day,mana=game.hero.mana;
 key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Town&&game.resources[Gold]==94000&&game.resources[Gems]==0&&game.town.pool[6]==2&&game.day==day&&game.hero.mana==mana,"Recruit modal did not settle exact multi-resource costs");
 check(game.validate(),"Town UI flow left invalid game state");
}
}
