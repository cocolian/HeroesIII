#include "h3/localization.hpp"
#include <unordered_map>

namespace h3 {
namespace {
using Entry = std::pair<std::string_view,std::string_view>;
#include "../generated/map_text_zh.inc"
const std::unordered_map<std::string_view,std::string_view>& translations(){
 static const std::unordered_map<std::string_view,std::string_view> values(std::begin(MapTextZh),std::end(MapTextZh));
 return values;
}
}
std::string chineseMapText(std::string_view scenario,std::string_view source){
 const auto& values=translations();
 if(!scenario.empty()){auto at=values.find(std::string(scenario)+"\n"+std::string(source));if(at!=values.end())return std::string(at->second);}
 auto at=values.find(source);return at==values.end()?std::string(source):std::string(at->second);
}
void localizeMapData(nlohmann::json& node,std::string_view scenario){
 if(node.is_array()){for(auto& value:node)localizeMapData(value,scenario);return;}
 if(!node.is_object())return;
 for(auto& item:node.items()){
  auto& value=item.value();const auto& key=item.key();
  if(value.is_string()&&(key=="name"||key=="title"||key=="variant"||key=="scenarioName"||key=="message"||key=="description"||key=="proposal"||key=="progress"||key=="completion"||key=="biography"||key=="bio"||key=="hero"))value=chineseMapText(scenario,value.get_ref<const std::string&>());
  else if((key=="log"||key=="artifacts")&&value.is_array()){for(auto&line:value)if(line.is_string())line=chineseMapText(scenario,line.get_ref<const std::string&>());}
  else if(value.is_structured())localizeMapData(value,scenario);
 }
}
}
