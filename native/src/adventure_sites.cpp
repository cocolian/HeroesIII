#include "h3/game.hpp"
#include <algorithm>

namespace h3 {
namespace {
bool active(const AdventureBonus&bonus,int day){return !bonus.expiresDay||day<bonus.expiresDay;}
std::string signedNumber(int value){return (value>0?"+":"")+std::to_string(value);}
}
std::string adventureSiteName(int sourceClass){
 switch(sourceClass){
 case 11:return "浮标";case 12:return "营火";case 14:return "天鹅湖";
 case 17:case 18:case 19:case 20:case 216:case 217:case 218:return "野外生物巢穴";
 case 23:return "马力特塔";case 28:return "仙女环";case 30:return "幸运泉";
 case 31:return "青春泉";case 32:return "真理花园";case 38:return "幸运神像";
 case 41:return "智慧图书馆";case 42:return "灯塔";case 48:return "魔法泉";case 51:return "雇佣兵营地";
 case 52:return "美人鱼";case 55:return "神秘花园";case 56:return "绿洲";
 case 61:return "星轴";case 64:return "集结旗";case 87:return "船厂";
 case 92:return "海妖";case 94:return "马厩";case 96:return "神庙";
 case 97:return "盗贼之家";case 100:return "学习石";case 102:return "知识之树";
 case 105:return "废弃货车";case 110:return "饮马池";
 default:return "地图物件（类型 "+std::to_string(sourceClass)+"）";
 }
}
int Game::adventureModifier(const Hero&who,int stat)const{
 int result=0;for(const auto&bonus:who.adventureBonuses)if(active(bonus,day))result+=stat==0?bonus.morale:stat==1?bonus.luck:bonus.movement;return result;
}
bool Game::hasAdventureBonus(const Hero&who,int sourceClass)const{
 return std::any_of(who.adventureBonuses.begin(),who.adventureBonuses.end(),[&](const auto&b){return b.sourceClass==sourceClass&&active(b,day);});
}
void Game::consumeAdventureBonuses(){
 auto consume=[](Hero&who){std::erase_if(who.adventureBonuses,[](const auto&b){return b.untilBattle;});};
 consume(hero);if(battle&&heroes.contains(battle->targetId))consume(heroes[battle->targetId].value);
}
std::optional<std::string> Game::visitAdventureSite(Object&object){
 if(object.type!=ObjectType::Site)return {};
 if(!object.properties.is_object())object.properties=nlohmann::json::object();
 int cls=object.sourceClass,week=(day-1)/7;
 if(cls==42){
  if(object.owner==currentOwner())return "这座灯塔已属于你。每座灯塔使所属玩家的英雄每天航海行动力 +500，可与其他灯塔叠加。";
  object.owner=currentOwner();object.visited=true;
  return "灯塔已归你控制。从明天起，所属玩家的所有英雄每天航海行动力 +500；多座灯塔可以叠加，不影响陆地移动。";
 }
 if(cls==97)return "盗贼向你提供各势力的完整情报。";
 if(cls==92){
  if(!hero.onBoat)return "需要乘船靠近海妖。";
  if(hasAdventureBonus(hero,92))return "船员已经用蜡封住耳朵，海妖的歌声无法再次诱惑他们。";
  hero.adventureBonuses.push_back({92,object.id,0,0,0,0,true});
  int64_t xp=0;std::string losses;
  for(auto&stack:hero.army)if(!stack.empty()&&stack.count>1){
   // Integer arithmetic preserves the classic ceil(30%) rule even for large stacks.
   int lost=(stack.count*3+9)/10;stack.count-=lost;xp+=int64_t(lost)*Units[stack.unit].hp;
   losses+=(losses.empty()?"":"、")+Units[stack.unit].name+" × "+std::to_string(lost);
  }
  if(!xp)return "海妖唱起诡异的歌声，但你的小队抵挡住了诱惑，没有士兵跳入海中。";
  int award=int(std::min<int64_t>(1000000000,xp));gainXP(award);
  return "海妖的歌声使部分士兵跳海溺亡。\n损失："+losses+"。\n你从这次遭遇中获得 "+std::to_string(award)+" 点经验。";
 }
 if(cls==48){
  if(hero.mana>=2*maxMana())return "魔法值已经达到上限的两倍，无需饮用泉水。";
  const char*key=hero.pos.x<object.pos.x?"springLeftWeek":"springWeek";
  if(object.properties.value(key,-1)==week)return "魔法泉本周已经枯竭，下周恢复。";
  hero.mana=2*maxMana();object.properties[key]=week;return "魔法泉将魔法值恢复到上限的两倍："+std::to_string(hero.mana)+"。";
 }
 if(cls==55){
  if(object.properties.value("gardenClaimWeek",-1)==week)return "神秘花园本周已经领过奖励，下周可以再次领取。";
  if(object.properties.value("gardenRewardWeek",-1)!=week){object.properties["gardenRewardWeek"]=week;object.properties["gardenResource"]=random()<.5?Gems:Gold;}
  int resource=object.properties["gardenResource"],quantity=resource==Gems?5:500;
  resources[resource]=int(std::min<int64_t>(1000000000,int64_t(resources[resource])+quantity));object.properties["gardenClaimWeek"]=week;
  return "神秘花园提供 "+std::to_string(quantity)+" "+ResourceNames[resource]+"。";
 }
 AdventureBonus bonus;bonus.sourceClass=cls;bonus.sourceId=object.id;int immediateMovement=0;bool loseMovement=false;
 switch(cls){
 case 11:bonus.morale=1;break;
 case 14:bonus.luck=2;loseMovement=true;break;
 case 28:bonus.luck=1;break;
 case 30:{
  if(!hasAdventureBonus(hero,cls)&&object.properties.value("fortuneWeek",-1)!=week){constexpr int values[]={-1,1,2,3};object.properties["fortuneWeek"]=week;object.properties["fortuneLuck"]=values[int(random()*4)];}
  bonus.luck=object.properties.value("fortuneLuck",1);break;
 }
 case 31:bonus.morale=1;immediateMovement=400;break;
 case 38:{int weekday=(day-1)%7+1;bonus.morale=weekday%2==0||weekday==7;bonus.luck=weekday%2==1;break;}
 case 52:bonus.luck=1;break;
 case 56:bonus.morale=1;immediateMovement=800;break;
 case 64:bonus.morale=bonus.luck=1;immediateMovement=400;break;
 case 94:bonus.movement=400;bonus.untilBattle=false;bonus.expiresDay=week*7+8;immediateMovement=400;break;
 case 96:bonus.morale=(day-1)%7==6?2:1;break;
 case 110:bonus.morale=1;immediateMovement=400;break;
 default:return {};
 }
 // Stables upgrade all Cavaliers on every visit, independently of the weekly movement reward.
 bool upgraded=false;if(cls==94)for(auto&stack:hero.army)if(stack.unit==Cavalier&&!stack.empty()){stack.unit=Champion;upgraded=true;}
 if(hasAdventureBonus(hero,cls))return (upgraded?std::string("骑兵已免费升级为骑士。\n"):std::string{})+(cls==94?"本周已经获得马厩的移动加成，不能重复领取。":"此类地点的加成仍然有效；下一场战斗结束后可以再次访问。");
 std::erase_if(hero.adventureBonuses,[&](const auto&b){return !active(b,day);});
 hero.adventureBonuses.push_back(bonus);
 if(loseMovement)hero.movement=0;
 if(cls==94){if(town.stableWeek==week)immediateMovement=0;hero.maxMovement=movementAllowance();}
 hero.movement=std::min(100000,hero.movement+immediateMovement);
 std::string message;
 if(bonus.morale)message+="士气"+signedNumber(bonus.morale)+"，";
 if(bonus.luck)message+="幸运"+signedNumber(bonus.luck)+"，";
 if(immediateMovement)message+="移动力+"+std::to_string(immediateMovement)+"，";
 if(loseMovement)message+="今日剩余移动力耗尽，";
 if(message.ends_with("，"))message.resize(message.size()-std::string("，").size());
 message+="。\n";
 message+=cls==94?"本周每天的陆地移动力+400。\n下周失效，同周不能叠加。":"加成持续到下一场战斗结束。\n同类地点不能叠加。";
 if(upgraded)message+="\n骑兵已免费升级为骑士。";
 return message;
}
}
