#include "h3/game.hpp"
#include <algorithm>

namespace h3 {
namespace {
constexpr int MaxResourceBalance=1000000000; // Current native save-state limit.
const std::array<std::array<TradeRate,8>,9> Rates={{
#include "../generated/market.inc"
}};
bool basic(int resource){return resource==Wood||resource==Ore;}
}
std::optional<TradeRate> marketRate(int sold,int bought,int marketplaces){
 if(sold<0||sold>=ResourceCount||bought<0||bought>=ResourceCount||sold==bought||marketplaces<=0)return {};
 int category=sold==Gold?(basic(bought)?0:1):basic(sold)?(bought==Gold?2:basic(bought)?3:4):(bought==Gold?5:basic(bought)?6:7);
 return Rates[std::min(marketplaces,9)-1][category];
}
int Game::marketplaces(int marketObject)const{
 if(marketObject==0){if(!imported())return town.built[Marketplace]?1:0;int markets=0;for(auto&o:objects)if(o.type==ObjectType::Town&&o.owner==actingOwner&&settlements.contains(o.id))markets+=(o.id==activeTownId?town:settlements.at(o.id)).built[Marketplace];return markets;}
 if(marketObject<0)return 0;
 auto post=std::find_if(objects.begin(),objects.end(),[&](const auto&object){return object.id==marketObject;});
 return post!=objects.end()&&(post->type==ObjectType::TradingPost||post->sourceClass==221)&&post->pos==hero.pos&&post->level==mapLevel?5:0;
}
int Game::tradeLimit(int sold,int bought,int marketObject)const{
 if(battle||report||result||!hero.levelChoices.empty())return 0;
 auto rate=marketRate(sold,bought,marketplaces(marketObject));if(!rate)return 0;
 if(resources[sold]<0||resources[bought]<0||resources[bought]>MaxResourceBalance)return 0;
 return std::min(resources[sold]/rate->give,(MaxResourceBalance-resources[bought])/rate->receive);
}
std::string Game::trade(int sold,int bought,int lots,int marketObject){
 if(battle||report||result||!hero.levelChoices.empty())return "请先完成当前战斗或升级";
 if(!marketplaces(marketObject))return marketObject==0?"需要先建造市场":"英雄必须访问贸易站才能交易";
 auto rate=marketRate(sold,bought,marketplaces(marketObject));if(!rate)return "请选择两种不同的资源";
 if(lots<=0)return "请选择交易数量";
 if(lots>tradeLimit(sold,bought,marketObject))return "资源不足或交易数量超出范围";
 int64_t payment=int64_t(rate->give)*lots,receipt=int64_t(rate->receive)*lots;
 resources[sold]-=int(payment);resources[bought]+=int(receipt);
 std::string result="交出 "+std::to_string(payment)+" "+ResourceNames[sold]+"，获得 "+std::to_string(receipt)+" "+ResourceNames[bought]+"。";
 message(result);return result;
}
}
