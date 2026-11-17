#include "h3/renderer.hpp"
#include <algorithm>

namespace h3 {
void Desktop::thievesGuildDialog(){
 originalImage("tprank.pcx",{0,0,800,600});
 auto players=game.thievesGuild();constexpr SDL_Color gold{236,213,130,255};
 for(size_t i=1;i<players.size();++i)frame("prstrips.def",0,int(i)-1,{250+66*int(i),7,66,540});
 for(size_t i=0;i<players.size();++i)centerText("第"+std::to_string(i+1)+"名",{253+66*int(i),9,62,26},17,gold);
 const char*labels[]={"城镇数量","英雄数量","金币","木材与矿石","稀有资源","已访问方尖碑","宝物数量","军队实力","每日收入"};
 const char*colors[]={"red","blue","tan","green","orange","purple","teal","pink"};
 const char*names[]={"红色","蓝色","褐色","绿色","橙色","紫色","青色","粉色"};
 auto colorIndex=[&](int owner){auto name=ownerName(owner);for(int i=0;i<8;++i)if(name==colors[i])return i;return 0;};
 for(int row=0;row<9;++row){
  centerText(labels[row],{25,40+32*row,220,25},15,gold);
  std::vector<const GuildPlayer*> order;for(const auto&p:players)order.push_back(&p);
  std::stable_sort(order.begin(),order.end(),[row](auto a,auto b){return a->totals[row]>b->totals[row];});
  int rank=0;for(size_t first=0;first<order.size();++rank){
   size_t last=first+1;while(last<order.size()&&order[last]->totals[row]==order[first]->totals[row])++last;
   int count=int(last-first);for(int k=0;k<count;++k){
    int length=std::min(4,count-(k/4)*4),x=259+66*rank+(48-length*12)/2+(k%4)*12;
    int y=41+32*row+(k/4)*9;frame("itgflags.def",0,colorIndex(order[first+k]->owner),{x,y,15,20});
   }
   first=last;
  }
 }
 centerText("最强英雄",{25,364,220,25},16,gold);
 centerText("电脑策略",{25,447,220,25},16,gold);
 centerText("最强生物",{25,499,220,25},16,gold);
 for(size_t i=0;i<players.size();++i){
  const auto&p=players[i];int x=253+66*int(i),color=colorIndex(p.owner);
  auto name=ownerName(p.owner);originalImage("pr"+(name=="pink"?std::string("rose"):name)+".pcx",{x,334,62,109});
  centerText(names[color],{x,336,62,20},12,gold);
  if(p.bestHero){
   heroPortrait(*p.bestHero,{x+7,358,48,33});const auto&h=*p.bestHero;
   const int stats[]={h.attack,h.defense,h.power,h.knowledge};const char*shortLabels[]={"攻","防","力","知"};
   for(int k=0;k<4;++k)text(std::string(shortLabels[k])+" "+std::to_string(stats[k]),x+6,393+11*k,9);
   button({x+4,357,54,77},"",[]{},true,true,"最强英雄："+h.name);
  }
  centerText(p.owner==0?"玩家":"电脑",{x,450,62,20},12);
  if(p.bestCreature>=0){portrait(p.bestCreature,{x+6,482,50,48});button({x+6,482,50,48},"",[]{},true,true,Units[p.bestCreature].name);}
 }
 text("盗贼之家 · 各势力完整情报（相同名次的旗帜并列）",12,558,12,gold);
 resourceBar(578,744);
 originalButton({748,556,48,40},"tpmage1.def",0,[this]{visitPopup_.reset();scene=Scene::Adventure;focus_=-1;mouseX_=mouseY_=-1;},true,-1,"关闭情报窗口，返回地图（Enter / Esc）");
}
}
