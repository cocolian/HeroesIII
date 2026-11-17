#include "h3/game.hpp"
#include <algorithm>
#include <map>
#include <tuple>

namespace h3 {
namespace {
#include "../generated/ground_layers.inc"
const AdventureTemplate Templates[]={
#include "../generated/adventure_templates.inc"
};
bool covered(const AdventureTemplate&t,Point anchor,Point point,bool visitable=false){
 int x=anchor.x-point.x,y=anchor.y-point.y;
 return x>=0&&x<8&&y>=0&&y<6&&((visitable?t.visitable:t.blocked)&(uint64_t(1)<<(y*8+x)));
}
Point anchor(const AdventureTemplate&t,Point visit){return {visit.x-t.visitOffset.x,visit.y-t.visitOffset.y};}
}
bool originalGroundLayer(std::string_view scenario,std::string_view sprite,Point point,int level){
 auto key=std::pair{scenario,sprite};
 if(std::binary_search(std::begin(GroundDefaults),std::end(GroundDefaults),key))return true;
 return std::binary_search(std::begin(GroundInstances),std::end(GroundInstances),std::tuple{scenario,sprite,point.x,point.y,level});
}
const AdventureTemplate& adventureTemplate(std::string_view name){
 auto it=std::lower_bound(std::begin(Templates),std::end(Templates),name,[](const auto&t,auto key){return t.asset<key;});
 if(it==std::end(Templates)||it->asset!=name)throw std::runtime_error("Missing original adventure footprint: "+std::string(name));
 return *it;
}
std::string Game::mapObjectAsset(const Object&o)const{
 static constexpr const char*resources[]={"avtgold0.def","avtwood0.def","avtore0.def","avtmerc0.def","avtsulf0.def","avtcrys0.def","avtgems0.def"};
 static constexpr const char*mines[]={"avmgold0.def","avmsawd0.def","avmore0.def","avmalch0.def","avmsulf0.def","avmcrys0.def","avmgems0.def"};
 static constexpr const char*towns[]={"avccasx0.def","avcramx0.def","avctowx0.def","avcinfx0.def","avcnecx0.def","avcdunx0.def","avcstrx0.def","avcftrx0.def","avchforx.def"};
 static const std::map<int,std::string> creatures={
#include "../generated/adventure_creatures.inc"
 };
 if(!o.sprite.empty())return o.sprite;
 switch(o.type){
  case ObjectType::Town:return towns[o.owner==0?town.faction:0];
  case ObjectType::Mine:return mines[o.resource];
  case ObjectType::Resource:return resources[o.resource];
  case ObjectType::Treasure:return "avtchst0.def";
  case ObjectType::Monster:if(!o.army[0].empty()){auto it=creatures.find(Units[o.army[0].unit].originalId);if(it!=creatures.end())return it->second;}return "avwmon1.def";
  case ObjectType::Shrine:return "avxl1sh0.def";
  case ObjectType::Well:return "avxwelg0.def";
  case ObjectType::Artifact:return "ava0008.def";
  case ObjectType::Obelisk:return "avxoblg.def";
  case ObjectType::TradingPost:return "avxpost0.def";
  case ObjectType::Windmill:return "avmwndd0.def";
  case ObjectType::WaterWheel:return "avmwwhl0.def";
 }return {};
}
std::vector<bool> Game::movementMask()const{
 std::vector<bool> allowed(mapWidth*mapHeight);
 for(int i=0;i<mapWidth*mapHeight;++i){const auto&t=tiles[mapLevel*mapWidth*mapHeight+i];allowed[i]=(t.terrain!=Terrain::Water||hero.onBoat)&&t.terrain!=Terrain::Rock&&!t.blocked;}
 if(imported()&&!hero.onBoat)for(const auto&o:objects)if(o.level==mapLevel&&o.type==ObjectType::Boat&&!tile(o.pos)->blocked)allowed[o.pos.y*mapWidth+o.pos.x]=true;
 if(imported())return allowed;
 auto stamp=[&](const AdventureTemplate&t,Point at){
  for(int i=0;i<48;++i)if((t.blocked&~t.visitable)&(uint64_t(1)<<i)){
   Point p{at.x-i%8,at.y-i/8};if(tile(p))allowed[p.y*mapWidth+p.x]=false;
  }
 };
 for(int y=0;y<mapHeight;++y)for(int x=0;x<mapWidth;++x){
  auto d=tile({x,y})->decor;if(d!=Decor::None)stamp(adventureTemplate(d==Decor::Tree?"avlpntr0.def":"avlmtgr5.def"),{x,y});
 }
 for(const auto&o:objects){const auto&t=adventureTemplate(mapObjectAsset(o));stamp(t,anchor(t,o.pos));}
 return allowed;
}
bool Game::passable(Point p)const{
 auto*t=tile(p);if(!t||t->terrain==Terrain::Rock||t->blocked||t->decor!=Decor::None)return false;
 if(imported()){if(t->terrain!=Terrain::Water||hero.onBoat)return true;auto*o=objectAt(p);return o&&o->type==ObjectType::Boat;}
 if(t->terrain==Terrain::Water)return false;
 // Only four original mountain mask cells can cover this point. Tree crowns
 // may overlap visually, but their original one-cell footprint stays intact.
 const Point mountainAnchors[]={{0,0},{1,0},{1,1},{2,1}};
 for(auto d:mountainAnchors)if(auto*n=tile({p.x+d.x,p.y+d.y});n&&n->decor==Decor::Mountain)return false;
 for(const auto&o:objects){const auto&g=adventureTemplate(mapObjectAsset(o));auto at=anchor(g,o.pos);if(covered(g,at,p)&&!covered(g,at,p,true))return false;}
 return true;
}
void Game::clearObstacleFootprints(const std::vector<Point>&points){
 for(int y=0;y<mapHeight;++y)for(int x=0;x<mapWidth;++x){auto*t=tile({x,y});if(t->decor==Decor::None)continue;
  const auto&g=adventureTemplate(t->decor==Decor::Tree?"avlpntr0.def":"avlmtgr5.def");
  if(std::any_of(points.begin(),points.end(),[&](Point p){return covered(g,{x,y},p);}))t->decor=Decor::None;
 }
}
}
