#include "h3/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace h3 {
namespace {
constexpr SDL_Color GoldColor{236,213,130,255},Muted{189,171,135,255},Brown{54,38,24,255};
bool inside(SDL_Rect r,Point p){return p.x>=r.x&&p.y>=r.y&&p.x<r.x+r.w&&p.y<r.y+r.h;}
}
Point Desktop::canvasLogicalSize()const{
 int w=0,h=0;SDL_GetWindowSize(window_,&w,&h);auto size=logicalCanvasSize(w,h,scene==Scene::Adventure);return {size[0],size[1]};
}
SDL_Rect Desktop::adventureMapRect()const{auto size=canvasLogicalSize();return {8,8,size.x-208,size.y-56};}
SDL_Rect Desktop::adventureMinimapRect()const{
 auto size=canvasLogicalSize();int scale=std::max(1,std::min(144/game.mapWidth,144/game.mapHeight));
 return {size.x-170+(144-game.mapWidth*scale)/2,27+(144-game.mapHeight*scale)/2,game.mapWidth*scale,game.mapHeight*scale};
}
std::array<float,2> Desktop::mapCamera()const{
 auto map=adventureMapRect();float halfW=map.w/(64*zoom_),halfH=map.h/(64*zoom_);
 return {halfW>=game.mapWidth/2.f?game.mapWidth/2.f:std::clamp(camX_,halfW,game.mapWidth-halfW),halfH>=game.mapHeight/2.f?game.mapHeight/2.f:std::clamp(camY_,halfH,game.mapHeight-halfH)};
}
Point Desktop::mapScreen(Point p)const{auto map=adventureMapRect();auto camera=mapCamera();return {int(map.x+(p.x+.5-camera[0])*32*zoom_+map.w/2),int(map.y+(p.y+.5-camera[1])*32*zoom_+map.h/2)};}
Point Desktop::mapPoint(Point p)const{auto map=adventureMapRect();auto camera=mapCamera();return {int(std::floor((p.x-map.x-map.w/2)/(32*zoom_)+camera[0])),int(std::floor((p.y-map.y-map.h/2)/(32*zoom_)+camera[1]))};}
void Desktop::toggleFullscreen(){
 bool next=(SDL_GetWindowFlags(window_)&SDL_WINDOW_FULLSCREEN_DESKTOP)==0;
 if(SDL_SetWindowFullscreen(window_,next?SDL_WINDOW_FULLSCREEN_DESKTOP:0)!=0){say(std::string("切换全屏失败：")+SDL_GetError());return;}
 fullscreen_=next;mouseX_=mouseY_=-1;scrollTick_=0;mouseLeftDown_=minimapDragging_=marketDragging_=recruitDragging_=drag_=false;armyDrag_.reset();optionsDragging_=-1;pressedButton_=-1;nativePointer_.reset();
}
void Desktop::resetRenderTextures(bool all){
 if(canvas_.texture)SDL_DestroyTexture(canvas_.texture);canvas_={};
 if(all){clearArtworkCache();for(auto&[key,value]:images_)if(value.texture)SDL_DestroyTexture(value.texture);images_.clear();for(auto&[key,value]:texts_)if(value.texture)SDL_DestroyTexture(value.texture);texts_.clear();}
}
void Desktop::adventureFrame(){
 auto size=canvasLogicalSize();int width=size.x,height=size.y,side=width-192;
 if(width==800&&height==600){if(!originalImage("advmap.pcx",{0,0,800,600}))art("references/adventure-original.png",{0,0,800,600},{},false);return;}
 box({0,0,width,height},Brown);
 auto draw=[&](SDL_Rect source,SDL_Rect destination){originalImage("advmap.pcx",destination,source);};
 auto tile=[&](SDL_Rect source,SDL_Rect destination){
  for(int y=0;y<destination.h;y+=source.h)for(int x=0;x<destination.w;x+=source.w){int w=std::min(source.w,destination.w-x),h=std::min(source.h,destination.h-y);draw({source.x,source.y,w,h},{destination.x+x,destination.y+y,w,h});}
 };
 // Frame corners and buttons retain their size. Only empty leather/blue
 // areas extend; repeated thin border sections do not stretch ornaments.
 draw({0,0,32,32},{0,0,32,32});draw({576,0,32,32},{side-32,0,32,32});
 draw({0,522,32,32},{0,height-78,32,32});draw({576,522,32,32},{side-32,height-78,32,32});
 tile({32,0,544,8},{32,0,side-64,8});tile({0,32,8,490},{0,32,8,height-110});tile({600,32,8,490},{side-8,32,8,height-110});tile({32,546,544,8},{32,height-54,side-64,8});
 draw({0,554,8,24},{0,height-46,8,24});draw({8,554,592,24},{8,height-46,side-16,24});draw({600,554,8,24},{side-8,height-46,8,24});
 draw({608,0,192,399},{side,0,192,399});
 if(height>600){draw({616,400,176,170},{side+8,399,176,height-600});tile({608,400,8,170},{side,399,8,height-600});tile({792,400,8,170},{side+184,399,8,height-600});}
 draw({608,399,192,201},{side,height-201,192,201});
}
void Desktop::adventureRoute(){
 if(!settings_.showPath)return;
 auto arrows=route_;
 if(!arrows.empty()&&destination_){auto*target=game.objectAt(*destination_);if(target&&target->type==ObjectType::Monster&&Point{arrows.back().x,arrows.back().y}!=*destination_)arrows.push_back({destination_->x,destination_->y,0});}
 int spent=0;Point previous=game.hero.pos;
 for(size_t i=0;i<arrows.size();++i){
  const auto&step=arrows[i];Point current{step.x,step.y};spent+=step.cost;
  int index=0; // The last tile uses the original destination cross.
  if(i+1<arrows.size()){
   Point incoming{current.x-previous.x,current.y-previous.y};
   Point outgoing{arrows[i+1].x-current.x,arrows[i+1].y-current.y};
   // ADAG contains eight clockwise directions, starting north, in three
   // banks: left turns, straight arrows, and right turns.
   const Point directions[]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
   auto found=std::find(std::begin(directions),std::end(directions),outgoing);
   if(found!=std::end(directions)){
    int turn=incoming.x*outgoing.y-incoming.y*outgoing.x;
    index=(turn<0?1:turn>0?17:9)+int(found-std::begin(directions));
   }
  }
  if(spent>game.hero.movement)index+=25;
  auto p=mapScreen(current);int size=int(std::lround(32*zoom_));
  frame("adag.def",0,index,{p.x-size/2,p.y-size/2,size,size});
  previous=current;
 }
}
void Desktop::adventureHero(){
 if(game.imported()&&!game.activeHeroId)return;
 auto p=mapScreen(game.hero.pos);bool flip=heroDirection_.x<0;
 int group=heroDirection_.y>0?(heroDirection_.x?1:0):heroDirection_.y<0?(heroDirection_.x?3:4):2;
 if(marching_&&!route_.empty())group+=5;
 // Original hero and owner flag share a 96x64 canvas over 32px map tiles.
 // Its bottom edge meets the destination tile's bottom, not its center.
 auto scaled=[&](int n){return int(std::lround(n*zoom_));};
 SDL_Rect canvas{p.x-scaled(48),p.y-scaled(48),scaled(96),scaled(64)};
 frame(adventureHeroAsset(game.hero),group,int(animationTime_/80),canvas,flip);
 frame(game.hero.onBoat?ownerArt_["colors"][ownerName(0)].value("boatFlag",std::string("abf01g.def")):ownerHeroFlag(0),group,int(animationTime_/100),canvas,flip);
}
void Desktop::prepareFogFrames(){
 if(fogFramesReady_)return;
 fogFramesReady_=true;
 if(!frameCount("tshre.def"))return;
 const auto&animation=demo_["def"]["tshre.def"];
 const auto&frames=animation["groups"]["0"];
 std::vector<SDL_Surface*> pages;
 for(const auto&page:animation["pages"])pages.push_back(readSurface(page["file"],false,false));
 struct Candidate {FogFrame frame;std::array<int,8> alpha;};std::vector<Candidate> candidates;
 const Point samples[]={{1,1},{16,1},{30,1},{30,16},{30,30},{16,30},{1,30},{1,16}};
 for(size_t i=0;i<frames.size();++i){
  const auto&f=frames[i];auto*page=pages[f["page"].get<int>()];if(!page)continue;
  for(int transform=0;transform<4;++transform){
   Candidate candidate;candidate.frame={int(i),bool(transform&1),bool(transform&2)};
   for(size_t n=0;n<8;++n){
    int x=(candidate.frame.flipX?31-samples[n].x:samples[n].x)-f["offset"][0].get<int>();
    int y=(candidate.frame.flipY?31-samples[n].y:samples[n].y)-f["offset"][1].get<int>();
    int alpha=0;
    if(x>=0&&y>=0&&x<f["rect"][2].get<int>()&&y<f["rect"][3].get<int>()){
     x+=f["rect"][0].get<int>();y+=f["rect"][1].get<int>();
     alpha=static_cast<const Uint8*>(page->pixels)[y*page->pitch+x*4+3];
    }
    candidate.alpha[n]=alpha;
   }
   candidates.push_back(candidate);
  }
 }
 for(int mask=1;mask<256;++mask){
  std::array<int,8> expected;
  for(int n=0;n<8;++n){
   bool visible=mask&(1<<n);
   if(n%2)expected[n]=visible?0:255;
   else{
    bool a=mask&(1<<((n+7)%8)),b=mask&(1<<((n+1)%8));
    expected[n]=a&&b?0:(visible||a||b)?64:255;
   }
  }
  int best=std::numeric_limits<int>::max();
  for(const auto&candidate:candidates){
   int score=0;for(int n=0;n<8;++n){int d=candidate.alpha[n]-expected[n];score+=d*d*(n%2?4:1);}
   if(score<best){best=score;fogFrames_[mask]=candidate.frame;}
  }
 }
 for(auto*page:pages)if(page)SDL_FreeSurface(page);
}
void Desktop::adventureFog(){
 prepareFogFrames();auto clip=adventureMapRect();int size=int(std::lround(32*zoom_));
 const Point neighbors[]={{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0}};
 auto camera=mapCamera();int firstX=std::max(0,int(std::floor(camera[0]-clip.w/(64*zoom_)))-1),lastX=std::min(game.mapWidth,int(std::ceil(camera[0]+clip.w/(64*zoom_)))+1);int firstY=std::max(0,int(std::floor(camera[1]-clip.h/(64*zoom_)))-1),lastY=std::min(game.mapHeight,int(std::ceil(camera[1]+clip.h/(64*zoom_)))+1);
 for(int y=firstY;y<lastY;++y)for(int x=firstX;x<lastX;++x){
  if(game.tile({x,y})->seen)continue;
  auto p=mapScreen({x,y}),next=mapScreen({x+1,y+1});SDL_Rect tile{p.x-size/2,p.y-size/2,next.x-p.x,next.y-p.y};if(!SDL_HasIntersection(&tile,&clip))continue;
  unsigned mask=0;for(int i=0;i<8;++i){auto*t=game.tile({x+neighbors[i].x,y+neighbors[i].y});if(t&&t->seen)mask|=1u<<i;}
  if(!mask){
   // Full shroud must stay opaque even at fractional map zoom.
   box(tile,{0,0,0,255});frame("tshrc.def",0,int((unsigned(x)*73u^unsigned(y)*151u)%4),tile);
  }
  else{const auto&f=fogFrames_[mask];frame("tshre.def",0,f.index,tile,f.flipX,false,f.flipY);}
 }
}
void Desktop::adventure(){
 auto camera=mapCamera();camX_=camera[0];camY_=camera[1];
 auto sizeCanvas=canvasLogicalSize();int side=sizeCanvas.x-192,dx=sizeCanvas.x-800,dy=sizeCanvas.y-600;
 adventureFrame();SDL_Rect clip=adventureMapRect();SDL_RenderSetClipRect(renderer_,&clip);box(clip,{0,0,8,255});
 adventureTerrain();adventureObjects();adventureFog();SDL_RenderSetClipRect(renderer_,nullptr);
 // Keep minimap cells square and show the actual expanded viewport.
 box({side+22,27,144,144},{0,0,0,255});auto mini=adventureMinimapRect();int miniScale=mini.w/game.mapWidth;
 for(int y=0;y<game.mapHeight;y++)for(int x=0;x<game.mapWidth;x++){auto*t=game.tile({x,y});if(!t->seen)continue;SDL_Color c=t->terrain==Terrain::Water?SDL_Color{28,67,72,255}:t->terrain==Terrain::Sand?SDL_Color{164,151,100,255}:SDL_Color{70,99,44,255};box({mini.x+x*miniScale,mini.y+y*miniScale,miniScale,miniScale},c);}
 for(const auto&o:game.objects)if(o.level==game.mapLevel&&(o.type==ObjectType::Town||o.type==ObjectType::Mine||o.sourceClass==42||(Game::dwellingLocation(o.sourceClass)&&o.sourceClass!=78))&&game.tile(o.pos)->seen)box({mini.x+o.pos.x*miniScale,mini.y+o.pos.y*miniScale,miniScale,miniScale},ownerColor(o.owner));
 int left=std::clamp(int(std::floor(camX_-clip.w/(64*zoom_))),0,game.mapWidth-1),right=std::clamp(int(std::ceil(camX_+clip.w/(64*zoom_))),left+1,game.mapWidth);
 int top=std::clamp(int(std::floor(camY_-clip.h/(64*zoom_))),0,game.mapHeight-1),bottom=std::clamp(int(std::ceil(camY_+clip.h/(64*zoom_))),top+1,game.mapHeight);
 outline({mini.x+left*miniScale,mini.y+top*miniScale,(right-left)*miniScale,(bottom-top)*miniScale},{211,45,36,255});
 box({mini.x+game.hero.pos.x*miniScale,mini.y+game.hero.pos.y*miniScale,miniScale,miniScale},ownerColor(0));
 heroPortrait(game.hero,{619+dx,212,48,32});
 int movementHeight=int(30.*std::clamp(game.hero.movement/double(std::max(1,game.hero.maxMovement)),0.,1.));
 int manaHeight=int(30.*std::clamp(game.hero.mana/double(std::max(1,game.maxMana())),0.,1.));
 box({611+dx,213,4,30},{20,28,17,255});box({611+dx,243-movementHeight,4,movementHeight},{54,166,50,255});
 box({669+dx,213,4,30},{16,23,36,255});box({669+dx,243-manaHeight,4,manaHeight},{56,102,211,255});
 frame("itpa.def",0,2+game.town.faction*2+(game.town.built[Castle]?18:0),{747+dx,212,48,32});
 box({609+dx,399+dy,183,174},Brown);heroPortrait(game.hero,{615+dx,403+dy,56,64});text(game.hero.name,682+dx,403+dy,12,GoldColor);text("攻 防 力 知",676+dx,426+dy,10);
 text(std::to_string(game.primarySkill(0))+"  "+std::to_string(game.primarySkill(1))+"  "+std::to_string(game.primarySkill(2))+"  "+std::to_string(game.primarySkill(3)),681+dx,445+dy,11);
 for(int i=0;i<7;i++){auto&s=game.hero.army[i];SDL_Rect r{615+dx+i%4*43,474+dy+i/4*47,41,44};outline(r,Muted);if(!s.empty()){portrait(s.unit,{r.x+1,r.y+1,39,40});text(std::to_string(s.count),r.x+3,r.y+30,10);}}
 auto heroIds=game.ownedHeroes();heroListPage_=std::clamp(heroListPage_,0,std::max(0,(int(heroIds.size())-1)/5));for(size_t row=0;row<5&&heroListPage_*5+row<heroIds.size();++row){int id=heroIds[heroListPage_*5+row];SDL_Rect rect{615+dx,212+int(row)*32,56,32};heroPortrait(game.heroes[id].value,{rect.x,rect.y,48,32});text(std::to_string(row+1),rect.x+43,rect.y+17,9);if(id==game.activeHeroId)outline(rect,GoldColor);button(rect,"",[this,id]{activateHero(id);},true,true,game.heroes[id].value.name+(game.heroes[id].sleeping?"（休眠）":"")+"：点击选择并居中此英雄");}
 if(!heroIds.empty()){button({615+dx,197,56,14},"",[this]{--heroListPage_;},heroListPage_>0,true,"上一页英雄");button({615+dx,380,56,14},"",[this]{++heroListPage_;},(heroListPage_+1)*5<int(heroIds.size()),true,"下一页英雄");}
 std::vector<int>townIds;if(game.imported())for(const auto&o:game.objects)if(o.type==ObjectType::Town&&o.owner==0)townIds.push_back(o.id);townListPage_=std::clamp(townListPage_,0,std::max(0,(int(townIds.size())-1)/5));for(int row=0;row<5&&townListPage_*5+row<int(townIds.size());++row){int id=townIds[townListPage_*5+row];const auto&t=id==game.activeTownId?game.town:game.settlements.at(id);SDL_Rect rect{749+dx,212+row*32,42,32};frame("itpa.def",0,2+t.faction*2+(t.built[Castle]?18:0),rect);if(id==game.activeTownId)outline(rect,GoldColor);button(rect,"",[this,id]{game.selectTown(id);scene=Scene::Town;focus_=-1;},true,true,"管理城镇："+game.objectById(id)->name);}
 if(!townIds.empty()){button({749+dx,197,42,14},"",[this]{--townListPage_;},townListPage_>0,true,"上一页城镇");button({749+dx,380,42,14},"",[this]{++townListPage_;},(townListPage_+1)*5<int(townIds.size()),true,"下一页城镇");}
 std::string rollover;
 auto centerHero=[this]{camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;mouseX_=mouseY_=-1;};
 if(heroIds.empty())button({615+dx,212,56,32},"",[this]{scene=Scene::Hero;},true,true,"英雄详情：属性、部队、阵型和魔法书（H）");
 if(townIds.empty())button({749+dx,212,42,32},"",[this]{if(game.hasTown())scene=Scene::Town;},game.hasTown(),true,"进入城镇界面，建设与招募部队（T）");
 auto control=[&](SDL_Rect original,const std::string&label,std::function<void()> action,bool enabled=true){SDL_Rect rect{original.x+dx,original.y,original.w,original.h};if(!enabled)box(rect,{0,0,0,115});button(rect,"",std::move(action),enabled,true,label);if(inside(rect,{mouseX_,mouseY_}))rollover=label;};
 control({680,197,31,30},"王国总览：查看英雄、城镇、驻军与日收入",[this]{scene=Scene::Overview;overviewPage_=0;});
 control({712,197,31,30},game.mapLevels==2?"当前为"+std::string(game.mapLevel?"地下":"地上")+"层；通过地下通道抵达另一层":"地上/地下切换：本地图没有地下层",[]{},false);
 control({680,229,31,30},"任务记录：查看已触发的地图剧情和奖励",[this]{scene=Scene::Journal;journalPage_=0;},game.imported());
 control({712,229,31,30},"英雄休眠：切换当前英雄的休眠状态",[this]{if(game.heroes.contains(game.activeHeroId))game.heroes[game.activeHeroId].sleeping=!game.heroes[game.activeHeroId].sleeping;persist();},game.imported());
 control({680,261,31,30},"移动英雄（M）：先点击地图规划路线，再点击此按钮行军",[this]{marching_=!route_.empty();},!route_.empty());
 control({712,261,31,30},"打开魔法书（C）；需要先在魔法行会购买魔法书",[this]{openSpellbook();},game.hero.spellbook);
 control({680,293,31,30},"藏宝图（P）：查看方尖碑揭开的圣杯位置；在目标格按 D 挖掘",[this]{scene=Scene::Puzzle;marching_=false;});
 control({712,293,31,30},"系统选项",[this]{openOptions();});
 control({680,325,63,28},"选择下一位英雄（N）",[this,centerHero]{auto ids=game.ownedHeroes();std::erase_if(ids,[this](int id){return game.heroes[id].sleeping;});if(ids.size()<2){centerHero();return;}auto at=std::find(ids.begin(),ids.end(),game.activeHeroId);activateHero(at==ids.end()||++at==ids.end()?ids.front():*at);});
 control({680,358,63,29},"结束回合（E）：结算城镇及矿山收入，恢复行动力",[this]{say(game.endDay());persist();settle();},!game.result);
 if(rollover.empty()&&inside(clip,{mouseX_,mouseY_}))if(auto*object=adventureObjectAt({mouseX_,mouseY_})){
  rollover=object->name;
  bool rogues=std::any_of(game.hero.army.begin(),game.hero.army.end(),[](auto&s){return !s.empty()&&(creatureAbility(s.unit).flags&1024);});int visions=rogues?3:game.adventureSpellActive(Visions)?game.hero.adventureSpellMastery[Visions]:-1;if(visions>=0&&(rogues||std::hypot(object->pos.x-game.hero.pos.x,object->pos.y-game.hero.pos.y)<=std::max(3,game.primarySkill(2)*std::max(1,visions)))){if(object->type==ObjectType::Monster){auto reaction=game.neutralReaction(object->id);rollover+=" · "+std::to_string(armyCount(object->army))+" 名 · "+(reaction.intent==NeutralIntent::JoinFree?"愿意加入":reaction.intent==NeutralIntent::JoinPaid?"要求金币后加入":reaction.intent==NeutralIntent::Flee?"准备逃走":"准备战斗");}else if((object->type==ObjectType::EnemyHero&&visions>=2)||(object->type==ObjectType::Town&&visions==3)){if(game.heroes.contains(object->id)){const auto&h=game.heroes.at(object->id).value;if(h.adventureSpellDay[Disguise]==game.day)rollover+=" · 部队经过伪装";else rollover+=" · "+std::to_string(h.level)+" 级 · "+std::to_string(armyCount(h.army))+" 名部队";}else rollover+=" · 驻军 "+std::to_string(armyCount(object->army));}}

  if(object->type==ObjectType::Windmill||object->type==ObjectType::WaterWheel)rollover+=object->visited?"（本周已领取）":"（本周尚未领取）";
  if(object->type==ObjectType::Well)rollover+=game.hero.wellDay==game.day?"（今日已饮用魔法井）":"（恢复魔法，每日限一次）";
  if(object->type==ObjectType::Obelisk)rollover+=object->visited?"（已拜访）":"（尚未拜访）";
  if(object->type==ObjectType::Shrine)rollover+="：传授一级法术，需要魔法书";
  if(object->sourceClass==42)rollover+=(object->owner==0?"（己方）":"（未占领）")+std::string("：航海行动力 +500 / 日");
  if(object->type==ObjectType::Mine)rollover+=(object->owner==0?"（己方）":"（未占领）")+std::string("：每日 +")+std::to_string(Game::mineYield(object->resource))+" "+ResourceNames[object->resource];
 }
 box({9,sizeCanvas.y-45,clip.w-2,18},Brown);
 text(!rollover.empty()?rollover:toastUntil_>SDL_GetTicks64()?toast_:game.log.empty()?"冒险地图":game.log.back(),14,sizeCanvas.y-43,10,GoldColor,clip.w-12);
 resourceBar(sizeCanvas.y-22,sizeCanvas.x-200);
 if(!originalImage("aresbar.pcx",{606+dx,sizeCanvas.y-22,189,19},SDL_Rect{595,0,199,22}))box({606+dx,sizeCanvas.y-22,189,19},{132,18,12,255});
 text("第 "+std::to_string((game.day-1)/28+1)+" 月  第 "+std::to_string((game.day-1)%28/7+1)+" 周  第 "+std::to_string((game.day-1)%7+1)+" 日",613+dx,sizeCanvas.y-20,10);
}
}
