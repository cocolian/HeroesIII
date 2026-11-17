#include "h3/game.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace h3;
int main(){int checks=0;try{
 auto check=[&](bool ok,const char*m){if(!ok)throw std::runtime_error(m);++checks;};auto root=std::filesystem::path(H3_TEST_SOURCE_ROOT);
 for(const auto&file:std::filesystem::directory_iterator(root/"assets/demo/scenarios"))if(file.path().extension()==".json"){
  nlohmann::json data;std::ifstream(file.path())>>data;auto id=data["id"].get<std::string>();
  for(const auto&o:data["objects"]){const auto&t=data["templates"][o["template_idx"].get<int>()];auto sprite=t["def"].get<std::string>();std::transform(sprite.begin(),sprite.end(),sprite.begin(),[](unsigned char c){return char(std::tolower(c));});auto&c=o["coordinates"];
   check(originalGroundLayer(id,sprite,{c["x"],c["y"]},c["z"])==(t["is_ground"].get<int>()!=0),"Legacy drawing layer differs from its original H3M instance");
  }
 }
 Game g;std::string error;check(g.importScenario(root/"assets/demo/scenarios/wayfarer-1.json",error),"Wayfarer import failed");
 int cliffs=0;for(const auto&d:g.decorations)if(d.sprite.starts_with("avxcf")){++cliffs;check(d.ground,"Walkable cliff overlay was imported as foreground");}check(cliffs>0,"Reported cliff assets were not tested");
 auto original=g.toJson(),legacy=original;for(auto&d:legacy["decorations"])d.erase("ground");for(auto&o:legacy["objects"])o.erase("ground");
 auto path=std::filesystem::temp_directory_path()/("heroesiii-ground-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");{std::ofstream f(path);f<<legacy;}Game loaded;check(loaded.load(path,error)&&loaded.toJson()==original,"Existing progress did not regain its original drawing layers");
 legacy=original;legacy["decorations"][0]["ground"]=!legacy["decorations"][0]["ground"].get<bool>();{std::ofstream f(path);f<<legacy;}check(loaded.load(path,error)&&loaded.toJson()==legacy,"Explicit per-instance drawing flag was overwritten");std::filesystem::remove(path);
 std::cout<<checks<<" ground-layer checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
