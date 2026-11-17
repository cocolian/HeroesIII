#include "h3/game.hpp"
#include "h3/localization.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace h3;
int main(){int checks=0;try{
 auto check=[&](bool value,const std::string&message){if(!value)throw std::runtime_error(message);++checks;};
 const auto root=std::filesystem::path(H3_TEST_SOURCE_ROOT);nlohmann::json catalog;std::ifstream(root/"assets/demo/localization/zh-CN.json")>>catalog;
 for(const auto&item:catalog["common"].items())check(chineseMapText({},item.key())==item.value().get<std::string>(),"Common Chinese catalog is stale");
 for(const auto&map:catalog["maps"].items())for(const auto&item:map.value().items())check(chineseMapText(map.key(),item.key())==item.value().get<std::string>(),"Compiled map translation is stale");
 auto read=[&](const std::string&id){nlohmann::json j;std::ifstream(root/"assets/demo/scenarios"/(id+".json"))>>j;return j;};
 auto raw=read("lord-of-the-rings-1"),localized=raw;localizeMapData(localized,"lord-of-the-rings-1");
 auto mechanics=[](auto&& self,nlohmann::json&j)->void{if(j.is_object()){for(auto key:{"name","title","description","message","proposal","progress","completion","biography","bio","hero","scenarioName","variant"})j.erase(key);for(auto&v:j)self(self,v);}else if(j.is_array())for(auto&v:j)self(self,v);};
 auto beforeRules=raw,afterRules=localized;mechanics(mechanics,beforeRules);mechanics(mechanics,afterRules);check(beforeRules==afterRules,"Chinese display changed map mechanics: "+nlohmann::json::diff(beforeRules,afterRules).dump());
 for(const auto&file:std::filesystem::directory_iterator(root/"assets/demo/scenarios"))if(file.path().extension()==".json"){
  nlohmann::json original;std::ifstream(file.path())>>original;auto translated=original;localizeMapData(translated,original["id"].get<std::string>());
  mechanics(mechanics,original);mechanics(mechanics,translated);check(original==translated,"Translation altered rules in "+file.path().filename().string());
 }
 for(size_t i=0;i<raw["objects"].size();++i)check(localized["objects"][i]["coordinates"]==raw["objects"][i]["coordinates"]&&localized["objects"][i]["template_idx"]==raw["objects"][i]["template_idx"],"Chinese text moved a map object");
 auto event=localized["timedEvents"][1]["message"].get<std::string>();check(event.find("矮人领主")!=std::string::npos&&event.find("九枚")!=std::string::npos&&event.find("Dwarf")==std::string::npos,"Reported LOTR prologue was not translated");
 Game g;std::string error;check(g.importScenario(root/"assets/demo/scenarios/lord-of-the-rings-1.json",error),error);check(g.hero.name=="希奥顿国王"&&g.validate(),"Imported hero name is still English");
 auto snapshot=g.toJson();snapshot["hero"]["name"]="King Theoden";snapshot["scenarioRules"]["timedEvents"]=raw["timedEvents"];snapshot["scenarioRules"]["journal"]=nlohmann::json::array({{{"day",1},{"hero","King Theoden"},{"title","Note"},{"message",raw["timedEvents"][1]["message"]}}});
 auto path=std::filesystem::temp_directory_path()/("heroesiii-localization-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");{std::ofstream f(path);f<<snapshot;}auto before=std::filesystem::file_size(path);Game loaded;check(loaded.load(path,error),error);check(loaded.hero.name=="希奥顿国王"&&loaded.scenarioRules["journal"][0]["message"]==event,"Existing English progress was not localized in memory");check(loaded.randomState==g.randomState&&loaded.resources==g.resources&&loaded.hero.pos==g.hero.pos&&loaded.day==g.day,"Localization changed gameplay progress");check(std::filesystem::file_size(path)==before,"Loading translations wrote the source save");std::filesystem::remove(path);
 check(chineseMapText("unknown-map","Uncatalogued original") == "Uncatalogued original","Unknown text was replaced with a misleading placeholder");
 auto twice=localized;localizeMapData(twice,"lord-of-the-rings-1");check(twice==localized,"Chinese localization is not idempotent");
 std::cout<<checks<<" localization checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
