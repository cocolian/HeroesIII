#include "h3/renderer.hpp"
#include <algorithm>

namespace h3 {
namespace {
constexpr int X=99,Y=3;
constexpr SDL_Color GoldColor{236,213,130,255},Muted{189,171,135,255};
constexpr std::array<int,7> ResourceOrder{Wood,Mercury,Ore,Sulfur,Crystal,Gems,h3::Gold};
bool contains(SDL_Rect rect,Point p){return p.x>=rect.x&&p.y>=rect.y&&p.x<rect.x+rect.w&&p.y<rect.y+rect.h;}
SDL_Rect cell(int side,int index){return {X+(side?321:39)+(index==6?83:index%3*83),Y+181+(index==6?160:index/3*80),68,68};}
}
void Desktop::originalButton(SDL_Rect rect,const std::string&name,int normal,std::function<void()>action,bool enabled,int disabled,const std::string& hint){
 bool pressed=enabled&&mouseLeftDown_&&pressedScene_==scene&&pressedButton_==int(buttons_.size())&&contains(rect,{mouseX_,mouseY_});
 int index=enabled?normal+int(pressed):disabled<0?normal:disabled;
 if(enabled&&normal==0&&frameCount(name,0)==4)index=pressed?1:contains(rect,{mouseX_,mouseY_})?3:0;
 frame(name,0,index,rect);if(!enabled&&disabled<0)box(rect,{0,0,0,130});
 buttons_.push_back({rect,std::move(action),enabled,{},hint.empty()?originalButtonHint(name,normal):hint});
}
void Desktop::marketIcon(int resource,SDL_Rect destination){
 auto index=std::find(ResourceOrder.begin(),ResourceOrder.end(),resource);
 if(index!=ResourceOrder.end())frame("resource.def",0,int(index-ResourceOrder.begin()),destination);
}
void Desktop::openMarket(int objectId){
 if(game.battle||game.report||game.result||!game.hero.levelChoices.empty()){say("请先完成当前战斗或升级。");return;}
 if(objectId==0&&scene==Scene::Adventure)if(const auto*o=game.objectAt(game.hero.pos);o&&o->type==ObjectType::TradingPost)objectId=o->id;
 if(!game.marketplaces(objectId)){say(objectId?"英雄必须访问贸易站才能交易。":"需要先建造市场。");return;}
 marketReturn_=scene==Scene::Town?Scene::Town:Scene::Adventure;
 marketObject_=objectId;marketSold_=marketBought_=-1;marketLots_=0;marketDragging_=false;marching_=false;focus_=-1;scene=Scene::Market;
}
SDL_Rect Desktop::marketTrack()const{return {X+247,Y+486,108,16};}
void Desktop::marketAmount(int mouseX){
 auto track=marketTrack();int span=track.w-16;
 int limit=game.tradeLimit(marketSold_,marketBought_,marketObject_);
 int position=std::clamp(mouseX-track.x-8,0,span);
 marketLots_=int((int64_t(position)*limit+span/2)/span);
}
void Desktop::market(){
 int savedX=mouseX_,savedY=mouseY_;mouseX_=mouseY_=-1000;
 if(marketReturn_==Scene::Town)town();else adventure();
 mouseX_=savedX;mouseY_=savedY;buttons_.clear();
 originalImage("tpmrkres.pcx",{X,Y,601,593});
 text(marketObject_?"贸易站":"资源市场",X+265,Y+12,16,GoldColor);
 text("王国资源",X+111,Y+142,12,GoldColor);text("可供交易",X+395,Y+142,12,GoldColor);
 int count=game.marketplaces(marketObject_);auto offer=marketRate(marketSold_,marketBought_,count);
 int limit=game.tradeLimit(marketSold_,marketBought_,marketObject_);marketLots_=std::clamp(marketLots_,0,limit);
 std::string description;
 if(offer)description="交出 "+std::to_string(offer->give)+" "+ResourceNames[marketSold_]+"，可以换取 "+std::to_string(offer->receive)+" "+ResourceNames[marketBought_]+"。";
 else description="选择希望交出的资源，以及希望换取的资源。";
 text(description,X+321,Y+54,12,GoldColor,250);
 text(marketObject_?"贸易站提供固定的五市场价格。":"王国中有 "+std::to_string(count)+" 个市场。",X+32,Y+68,12,GoldColor,250);
 text("选择资源，再拖动滑块决定交易数量。",X+32,Y+96,11,Muted,250);
 // Default Return closes the modal. Background buttons have been discarded.
 originalButton({X+521,Y+530,64,32},"iokay.def",0,[this]{scene=marketReturn_;marketDragging_=false;},true,2,"关闭市场并返回上一界面（Esc）");
 for(int side=0;side<2;side++)for(int index=0;index<ResourceCount;index++){
  int resource=ResourceOrder[index];auto rect=cell(side,index);
  marketIcon(resource,{rect.x+18,rect.y+10,32,32});
  std::string value=std::to_string(game.resources[resource]);
  if(side){auto rate=marketRate(marketSold_,resource,count);value=!rate?"—":rate->give==1?std::to_string(rate->receive):std::to_string(rate->receive)+"/"+std::to_string(rate->give);}
  text(value,rect.x+7,rect.y+50,10,side&&resource==marketSold_?Muted:GoldColor);
  bool selected=(side?marketBought_:marketSold_)==resource;
  if(selected)frame("tpmrkr3.def",0,0,{rect.x-3,rect.y-1,74,70});
  bool enabled=!side||marketSold_!=resource;
  buttons_.push_back({rect,[this,side,resource]{if(side)marketBought_=resource;else marketSold_=resource;marketLots_=0;if(marketSold_==marketBought_)marketBought_=-1;},enabled,{},std::string(side?"选择要买入的资源：":"选择要卖出的资源：")+ResourceNames[resource]});
 }
 if(marketSold_>=0)marketIcon(marketSold_,{X+140,Y+454,32,32});
 if(marketBought_>=0)marketIcon(marketBought_,{X+429,Y+454,32,32});
 text(offer?std::to_string(int64_t(offer->give)*marketLots_):"0",X+127,Y+497,11,GoldColor);
 text(offer?std::to_string(int64_t(offer->receive)*marketLots_):"0",X+416,Y+497,11,GoldColor);
 auto track=marketTrack();box(track,{18,17,14,255});outline(track,{127,113,77,255});
 int travel=track.w-16,knob=limit?int(int64_t(marketLots_)*travel/limit):0;
 frame("igpcrdiv.def",0,4,{track.x+knob,track.y,16,16});
 buttons_.push_back({track,[this]{marketAmount(mouseX_);},limit>0,{},"拖动滑块选择交易数量；← / → 微调，End 选最大数量"});
 originalButton({track.x-16,track.y,16,16},"igpcrdiv.def",0,[this]{marketLots_=std::max(0,marketLots_-1);},marketLots_>0);
 originalButton({track.x+track.w,track.y,16,16},"igpcrdiv.def",2,[this,limit]{marketLots_=std::min(limit,marketLots_+1);},marketLots_<limit);
 originalButton({X+223,Y+530,64,32},"tpmrkb.def",3,[this,limit]{marketLots_=limit;},limit>0,5);
 originalButton({X+302,Y+530,64,32},"tpmrkb.def",0,[this]{say(game.trade(marketSold_,marketBought_,marketLots_,marketObject_));marketLots_=0;persist();},marketLots_>0,2);
 originalButton({X+14,Y+530,64,32},"tpmrkbu1.def",0,[]{},false);
 text(toastUntil_>SDL_GetTicks64()?toast_:"选择资源并确认交易。",X+15,Y+574,10,GoldColor,565);
}
}
