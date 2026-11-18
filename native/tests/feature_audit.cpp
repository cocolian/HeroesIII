#include "h3/game.hpp"
#include <fstream>
#include <iostream>
using namespace h3;
int main(int argc,char**argv){if(argc!=2)return 2;using J=nlohmann::json;J spells=J::array(),buildings=J::array();
 for(int i=0;i<SpellCount;++i)if(!Spells[i].implemented)spells.push_back({{"id",i},{"name",Spells[i].name},{"adventure",Spells[i].adventure}});
 for(int f=0;f<FactionCount;++f)for(int b=0;b<BuildingCount;++b){const auto&d=TownDefs[f].buildings[b];if(d.available&&!d.implemented)buildings.push_back({{"faction",Factions[f].name},{"building",b},{"name",d.name}});}
 std::ofstream(argv[1])<<J{{"missingSpellEffects",spells},{"unimplementedAvailableBuildings",buildings},{"scope","Runtime flags; behavioral and UI audits are separate"}}.dump(2)<<'\n';
 std::cout<<spells.size()<<" missing spell effects, "<<buildings.size()<<" unavailable building services\n";
}
