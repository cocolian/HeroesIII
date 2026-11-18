#include "h3/locations.hpp"
#include <algorithm>
#include <cmath>

namespace h3::locations {
namespace {
void revealAt(Game&g,Point center,int level,int radius){for(int y=std::max(0,center.y-radius);y<std::min(g.mapHeight,center.y+radius+1);++y)for(int x=std::max(0,center.x-radius);x<std::min(g.mapWidth,center.x+radius+1);++x)if(std::hypot(x-center.x,y-center.y)<=radius){size_t i=(level*g.mapHeight+y)*g.mapWidth+x;g.tiles[i].seen=true;if(g.playerFog[g.currentOwner()].size()==g.tiles.size())g.playerFog[g.currentOwner()][i]=true;}}
bool allied(const Game&g,int a,int b){if(a==b)return true;const auto&rules=g.scenarioRules;if(!rules.contains("additional")||!rules["additional"].contains("teams"))return false;const auto&teams=rules["additional"]["teams"];if(!teams.value("num_teams",0)||!teams.contains("team_for_player"))return false;const char*names[]={"red","blue","tan","green","orange","purple","teal","pink"};int human=rules.value("originalPlayer",0);auto original=[human](int owner){return owner==0?human:owner<=human?owner-1:owner;};const auto&t=teams["team_for_player"];return t.value(names[original(a)],-1)==t.value(names[original(b)],-2);}
}
std::string pickup(Game&g,int id){auto*o=g.objectById(id);int cls=o->sourceClass;
 if(cls==108&&!o->visited&&!o->properties.value("locationSearchAccepted",false))return "一座古老的勇士之墓。搜索可能找到宝物，但会使部队士气降低 3 点，持续到下一场战斗。";
 if(cls==108)bonus(g,*o,-3,0);
 if(o->visited)return cls==108?"墓中已经空无一物。亵渎墓穴使部队士气 -3，持续到下一场战斗。":"这里已经被搜索过，没有剩余物品。";
 Resources reward{};int artifact=-1,dice=int(g.random()*100);
 if(cls==29){if(dice>=25)reward[Wood]=dice>=75?10:5;if(dice>=50)reward[Gold]=dice>=75?500:200;}
 else if(cls==86)artifact=g.rollArtifact(dice<55?66:dice<75?67:dice<95?68:69);
 else if(cls==22){if(dice<20)artifact=g.rollArtifact(dice<10?66:67);}
 else if(cls==39){int r=Wood+int(g.random()*6);reward[r]=1+int(g.random()*5);}
 else if(cls==105){if(dice<40)artifact=g.rollArtifact(dice<20?66:67);else if(dice<90)reward[Wood+int(g.random()*6)]=2+int(g.random()*4);}
 else if(cls==108)artifact=g.rollArtifact(dice<30?66:dice<80?67:dice<95?68:69);
 for(int r=0;r<ResourceCount;++r)g.resources[r]=int(std::min<int64_t>(1000000000,int64_t(g.resources[r])+reward[r]));
 if(artifact>=0)giveArtifact(g,artifact);
 o=g.objectById(id);o->visited=true;
 std::string result=artifact>=0?"获得宝物："+artifactName(artifact)+"。":resourcesText(reward).empty()?"没有找到可用的物资。":"获得 "+resourcesText(reward)+"。";
 if(cls==108)result+="\n部队士气 -3，持续到下一场战斗。";
 if(cls==29||cls==86){g.locationNotice(id,result);std::erase_if(g.objects,[id](auto&o){return o.id==id;});}
 return result;
}
std::string scout(Game&g,int id){auto&o=*g.objectById(id);int cls=o.sourceClass;
 if(cls==13){int type=std::clamp(o.sourceSubtype,0,2);auto&visits=g.scenarioRules["cartographers"];auto owner=std::to_string(g.currentOwner());
  bool visited=visits.is_object()&&visits.contains(owner)&&visits[owner].is_array()&&std::find(visits[owner].begin(),visits[owner].end(),type)!=visits[owner].end();
  return visited?"你已经购买过这类地图，无需再次付款。":std::string(type==0?"海洋":type==1?"地上陆地":"地下陆地")+"地图售价 1000 金币。购买后揭开对应区域的战争迷雾。";
 }
 if(cls==58||cls==60){revealAt(g,o.pos,o.level,20);o.visited=true;return "登高远眺，周围二十格内的地图已经揭开。";}
 if(cls==27)return "魔法师之眼将这里的景象传送给同色的魔法师小屋。拜访小屋可以查看所有同色魔眼附近的区域。";
 if(cls==37){int count=0;for(const auto&eye:g.objects)if(eye.sourceClass==27&&eye.sourceSubtype==o.sourceSubtype){revealAt(g,eye.pos,eye.level,10);++count;}o.visited=true;return "魔法师展示了 "+std::to_string(count)+" 处同色魔眼周围的景象，包括其他地图层。";}
 if(cls==15){
  for(int owner=0;owner<8;++owner)if(!allied(g,owner,g.currentOwner())&&g.playerFog[owner].size()==g.tiles.size()){
   auto&fog=g.playerFog[owner];for(int y=std::max(0,o.pos.y-20);y<std::min(g.mapHeight,o.pos.y+21);++y)for(int x=std::max(0,o.pos.x-20);x<std::min(g.mapWidth,o.pos.x+21);++x)if(std::hypot(x-o.pos.x,y-o.pos.y)<=20)fog[(o.level*g.mapHeight+y)*g.mapWidth+x]=false;
   // A kingdom can always see the immediate area around its own heroes and towns.
   auto retain=[&](Point p,int level,int radius){for(int y=std::max(0,p.y-radius);y<std::min(g.mapHeight,p.y+radius+1);++y)for(int x=std::max(0,p.x-radius);x<std::min(g.mapWidth,p.x+radius+1);++x)if(std::hypot(x-p.x,y-p.y)<=radius)fog[(level*g.mapHeight+y)*g.mapWidth+x]=true;};
   for(const auto&[heroId,h]:g.heroes)if(h.owner==owner)retain(h.value.pos,h.level,5+h.value.skills[Scouting]);
   for(const auto&town:g.objects)if(town.type==ObjectType::Town&&town.owner==owner)retain(town.pos,town.level,5);
  }
  o.visited=true;return "黑暗天幕重新遮蔽了其他势力在周围二十格内的视野。己方探索进度保留。";
 }
 return "";
}
}
