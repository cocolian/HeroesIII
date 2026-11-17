#include "h3/game.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace h3;
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("Usage: h3_map_object_audit OUTPUT.json");
 using Json=nlohmann::json;Json classes=Json::object();int maps=0;
 for(const auto&entry:std::filesystem::directory_iterator(std::filesystem::path(H3_TEST_SOURCE_ROOT)/"assets/demo/scenarios")){
  if(entry.path().extension()!=".json")continue;Game map;std::string error;
  if(!map.importScenario(entry.path(),error))throw std::runtime_error(error);++maps;
  for(const auto&o:map.objects)if(o.type==ObjectType::Site){
   auto key=std::to_string(o.sourceClass);auto&row=classes[key];
   if(row.is_null()){
    // Probe a rule after guards are removed, with sufficient resources. This
    // identifies the actual fallback dispatch; it is not a full rule-conformance test.
    Game p;p.scenarioId="object-rule-audit";p.mapWidth=map.mapWidth;p.mapHeight=map.mapHeight;p.mapLevels=map.mapLevels;p.mapLevel=o.level;p.tiles.resize(p.mapWidth*p.mapHeight*p.mapLevels);
    p.activeHeroId=1;p.hero.pos=o.pos;p.hero.onBoat=map.tiles[(o.level*map.mapHeight+o.pos.y)*map.mapWidth+o.pos.x].terrain==Terrain::Water;p.hero.army={Stack{Pikeman,100}};
    p.resources={100000,1000,1000,1000,1000,1000,1000};p.scenarioRules={{"originalPlayer",0},{"defeated",Json::array()},{"additional",{{"victory_condition",{{"type",255}}},{"loss_condition",{{"type",255}}}}}};
    Object copy=o;copy.army={};p.objects={copy};auto reply=p.visit(o.id);
    row={{"sourceClass",o.sourceClass},{"name",adventureSiteName(o.sourceClass)},{"genericFallback",reply.find("完整规则尚未实现")!=std::string::npos},{"instances",0},{"maps",Json::object()}};
   }
   row["instances"]=row["instances"].get<int>()+1;
   row["maps"][map.scenarioId]=row["maps"].value(map.scenarioId,0)+1;
  }
 }
 int missing=0,instances=0;for(const auto&row:classes)if(row["genericFallback"]){++missing;instances+=row["instances"].get<int>();}
 auto output=std::filesystem::path(argv[1]);std::filesystem::create_directories(output.parent_path());
 std::ofstream(output)<<Json{{"version","0.49.0"},{"maps",maps},{"scope","Site objects only; guard-free fallback dispatch, not original-rule completeness"},{"classes",classes},{"genericFallbackClasses",missing},{"genericFallbackInstances",instances}}.dump(2)<<'\n';
 std::cout<<maps<<" maps, "<<classes.size()<<" Site classes, "<<missing<<" classes still reach the generic fallback ("<<instances<<" objects)\n";
 return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
