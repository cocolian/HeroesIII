#include "h3/renderer.hpp"
#include "h3/locations.hpp"
#include <algorithm>

namespace h3 {
bool Desktop::locationDialog(){
 if(!visitPopup_)return false;int id=visitPopup_->objectId;auto*o=game.objectById(id);if(!o||!Game::extendedLocation(o->sourceClass))return false;
 auto offers=game.locationOffers(id);if(offers.empty())return false;
 if(locationObject_!=id){locationObject_=id;locationPage_=0;locationSelected_=-1;locationAmount_=1;}
 int pages=std::max(1,(int(offers.size())+3)/4);locationPage_=std::clamp(locationPage_,0,pages-1);
 if(locationSelected_>=int(offers.size())||locationSelected_<locationPage_*4||locationSelected_>=locationPage_*4+4)locationSelected_=-1;
 if(locationSelected_>=0)locationAmount_=std::clamp(locationAmount_,1,std::max(1,offers[locationSelected_].maximum));
 constexpr SDL_Color gold{236,213,130,255},muted{189,171,135,255};
 mapDialog({90,30,620,530});centerText(o->name,{110,48,580,32},24,gold);
 text(visitPopup_->message,120,92,13,{244,224,174,255},560);
 for(int row=0;row<4;++row){int index=locationPage_*4+row;if(index>=int(offers.size()))break;const auto&f=offers[index];int y=180+row*60;
  panel({119,y,562,54});if(index==locationSelected_)outline({119,y,562,54},gold);
  int tx=136;if(f.unit>=0){portrait(f.unit,{128,y+5,44,44});tx=184;}else if(f.artifact>=0){frame("artifact.def",0,f.artifact,{128,y+5,44,44});tx=184;}
  text(f.label,tx,y+6,15,f.enabled?gold:muted,385);
  auto cost=locations::resourcesText(f.cost);std::string detail=f.description;if(!cost.empty())detail+=std::string(detail.empty()?"":"；")+"单价 "+cost;
  text(detail,tx,y+29,10,muted,440);
  button({119,y,562,54},"",[this,index]{locationSelected_=index;locationAmount_=1;},f.enabled,true,f.label+(detail.empty()?"":"："+detail));
 }
 if(pages>1){button({119,426,78,25},"上一页",[this]{--locationPage_;locationSelected_=-1;},locationPage_>0);centerText(std::to_string(locationPage_+1)+" / "+std::to_string(pages),{220,426,360,25},12,gold);button({603,426,78,25},"下一页",[this]{++locationPage_;locationSelected_=-1;},locationPage_+1<pages);}
 if(locationSelected_>=0){const auto&f=offers[locationSelected_];
  if(f.maximum>1){button({119,461,30,28},"−",[this]{locationAmount_=std::max(1,locationAmount_-1);},locationAmount_>1);button({151,461,48,28},"最少",[this]{locationAmount_=1;});centerText(std::to_string(locationAmount_),{202,461,94,28},16,gold);button({299,461,30,28},"+",[this]{++locationAmount_;},locationAmount_<f.maximum);button({331,461,48,28},"最多",[this,maximum=f.maximum]{locationAmount_=maximum;});}
  Resources total{};for(int r=0;r<ResourceCount;++r)total[r]=int(std::min<int64_t>(1000000000,int64_t(f.cost[r])*locationAmount_));
  auto cost=locations::resourcesText(total);text(cost.empty()?"本次操作无需支付资源":"支付："+cost,119,498,12,gold,410);
  button({530,461,151,32},"确认",[this,id,f]{
   int amount=locationAmount_;locationSelected_=-1;locationAmount_=1;visitPopup_.reset();scene=Scene::Adventure;
   if(f.action==LocationAction::Trade)openMarket(id);else{say(game.locationAction(id,f.action,f.parameter,amount));settle();}
   persist();focus_=-1;
  },f.enabled,false,"确认当前选择；数量和价格已显示");
 }else text("请选择一项操作；可以关闭窗口取消。",119,472,13,muted,410);
 originalButton({617,511,64,32},"iokay.def",0,[this]{visitPopup_.reset();scene=Scene::Adventure;focus_=-1;locationSelected_=-1;locationObject_=0;},true,-1,"关闭地点窗口，返回地图（Enter / Esc）");
 return true;
}
}
