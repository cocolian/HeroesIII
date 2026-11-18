#include "h3/game.hpp"
#include "h3/locations.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace h3 {
std::vector<Step> Game::adventurePlan(int*targetId){
 if(targetId)*targetId=0;if(battle||report||result||hero.movement<=0)return {};
 const int n=mapWidth*mapHeight;std::vector<const Object*>at(n),guards(n);double own=armyPower(hero.army)*(1.+(primarySkill(0)+primarySkill(1))/30.);
 for(const auto&o:objects)if(o.level==mapLevel&&tile(o.pos)->seen){int i=o.pos.y*mapWidth+o.pos.x;if(o.type!=ObjectType::MapEvent&&(!at[i]||o.type==ObjectType::EnemyHero||o.type==ObjectType::Town))at[i]=&o;if(o.type==ObjectType::Monster&&o.owner!=actingOwner&&armyCount(o.army))for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){Point p{o.pos.x+dx,o.pos.y+dy};if(tile(p)&&(tile(p)->terrain==Terrain::Water)==(tile(o.pos)->terrain==Terrain::Water)){int j=p.y*mapWidth+p.x;if(!guards[j]||o.id<guards[j]->id)guards[j]=&o;}}}
 auto safe=[&](const Object*o){if(!o)return true;double strength=locations::estimatedGuards(*o);if(heroes.contains(o->id)){const auto&h=heroes.at(o->id).value;strength*=1.+(h.attack+h.defense)/30.;}if(o->type==ObjectType::Town&&o->owner!=actingOwner&&settlements.contains(o->id)){const auto&t=settlements.at(o->id);if(t.built[Fort]||t.built[Citadel]||t.built[Castle])strength*=1.35;if(t.built[Citadel]||t.built[Castle])strength+=700*(t.built[Castle]?2:1);}
 return strength*1.65<own;};
 // Couriers deliver recruits to the stronger allied hero before resuming exploration.
 const Hero* recipient=nullptr;double strongest=own*1.8;
 int keeper=-1;for(int slot=0;slot<ArmySlots;++slot)if(!hero.army[slot].empty()&&(keeper<0||Units[hero.army[slot].unit].speed>Units[hero.army[keeper].unit].speed))keeper=slot;
 if(armyCount(hero.army)>1)for(const auto&[id,h]:heroes)if(id!=activeHeroId&&h.owner==actingOwner&&h.level==mapLevel&&h.value.onBoat==hero.onBoat){bool deliverable=false;for(int slot=0;slot<ArmySlots;++slot){auto s=hero.army[slot];if(s.empty()||s.count<=(slot==keeper?1:0))continue;for(const auto&destination:h.value.army)if(destination.empty()||(destination.unit==s.unit&&destination.count<MaxArmyStack))deliverable=true;}double power=armyPower(h.value.army)*(1.+(h.value.attack+h.value.defense)/30.);if(deliverable&&power>strongest){strongest=power;recipient=&h.value;}}
 constexpr int Inf=100000000;std::vector<int>dist(n*2,Inf),parent(n*2,-1);using Node=std::pair<int,int>;std::priority_queue<Node,std::vector<Node>,std::greater<Node>>queue;int start=(hero.onBoat?n:0)+hero.pos.y*mapWidth+hero.pos.x;dist[start]=0;queue.emplace(0,start);double best=0;int end=-1,object=0;
 while(!queue.empty()){
  auto[c,state]=queue.top();queue.pop();if(c!=dist[state])continue;int i=state%n,mode=state/n;Point p{i%mapWidth,i/mapWidth};const auto*o=at[i];const auto*g=guards[i];bool enemy=o&&(o->type==ObjectType::Town||o->type==ObjectType::EnemyHero)&&o->owner!=actingOwner;bool encounter=g||(enemy&&armyCount(o->army));
  if(state!=start){double value=0;int target=0;
   if(g){if(safe(g)){value=180;target=g->id;}}
   else if(o){target=o->id;switch(o->type){
    case ObjectType::Town:if(enemy&&safe(o))value=1600;else if(o->owner==actingOwner){auto t=settlements.find(o->id);if(t!=settlements.end()){Army useful{};for(const auto&s:t->second.garrison)if(!s.empty()&&std::any_of(hero.army.begin(),hero.army.end(),[&](auto&a){return a.empty()||a.unit==s.unit;}))for(auto&slot:useful)if(slot.empty()){slot=s;break;}if(armyPower(useful)>std::max(500.,own*.25))value=900;}}break;
    case ObjectType::EnemyHero:if(enemy&&!sanctuaryProtected(o->id)&&safe(o))value=800;break;
    case ObjectType::Mine:if(o->owner!=actingOwner&&safe(o))value=550;break;
    case ObjectType::Treasure:case ObjectType::Resource:if(safe(o))value=200;break;
    case ObjectType::Artifact:if(safe(o))value=300;break;
    case ObjectType::Boat:if(!hero.onBoat)value=80;break;
    case ObjectType::Portal:if(o->properties.contains("destination"))value=o->visited?5:40;break;
    case ObjectType::Gate:if(questReady(*o))value=500;break;
    case ObjectType::Windmill:case ObjectType::WaterWheel:if(!o->visited)value=160;break;
    case ObjectType::Well:if(hero.mana<maxMana()/2&&hero.wellDay!=day)value=100;break;
    case ObjectType::Site:if(bankLocation(o->sourceClass)){auto offers=locationOffers(o->id);if((!o->properties.is_object()||!o->properties.value("locationCleared",false))&&safe(o)&&((!o->properties.is_object()||!o->properties.value("locationBankReady",false))||armyCount(o->army)>0||std::any_of(offers.begin(),offers.end(),[](const auto&f){return f.enabled;})))value=500;}else if(dwellingLocation(o->sourceClass)&&safe(o)){auto offers=locationOffers(o->id);if(o->owner!=actingOwner||std::any_of(offers.begin(),offers.end(),[](const auto&f){return f.action==LocationAction::Hire&&f.enabled;}))value=400;}else if(extendedLocation(o->sourceClass)&&!o->visited&&o->sourceClass!=2&&o->sourceClass!=50&&o->sourceClass!=80&&safe(o))value=150;else if(o->sourceClass==42&&o->owner!=actingOwner&&safe(o))value=350;else if(o->sourceClass==87&&!hero.onBoat&&resources[Gold]>=1000&&resources[Wood]>=10&&std::none_of(objects.begin(),objects.end(),[&](auto&b){return b.type==ObjectType::Boat&&b.level==o->level&&std::max(std::abs(b.pos.x-o->pos.x),std::abs(b.pos.y-o->pos.y))<=5;}))value=650;else if(!o->visited&&(o->sourceClass==12||o->sourceClass==23||o->sourceClass==32||o->sourceClass==51||o->sourceClass==61||o->sourceClass==100))value=180;break;
    default:break;}}
   if(!encounter&&(!o||o->type!=ObjectType::Gate)){int unknown=0;for(int dy=-5;dy<=5;dy+=2)for(int dx=-5;dx<=5;dx+=2)if(dx*dx+dy*dy<30)if(auto*t=tile({p.x+dx,p.y+dy});t&&!t->seen&&t->terrain!=Terrain::Rock)++unknown;value+=unknown*18;}
   double score=value/(c+450.);if(recipient&&std::max(std::abs(p.x-recipient->pos.x),std::abs(p.y-recipient->pos.y))<=1)score+=20000;if(target&&target==hero.adventureGoal&&value>0)score+=10000;if(score>best){best=score;end=state;object=target;}
   if(encounter||(o&&((o->type==ObjectType::Town&&enemy)||o->type==ObjectType::Gate||o->type==ObjectType::Portal||o->type==ObjectType::EnemyHero||locationBlocksPassage(*o))))continue;
  }
  for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){Point next{p.x+dx,p.y+dy};auto*t=tile(next);if(!t||!t->seen||(!dx&&!dy)||t->blocked||t->decor!=Decor::None||t->terrain==Terrain::Rock)continue;
   if(actingOwner>0&&heroes.contains(activeHeroId)){const auto&h=heroes.at(activeHeroId);if(h.patrolRadius>=0&&std::max(std::abs(next.x-h.patrolOrigin.x),std::abs(next.y-h.patrolOrigin.y))>h.patrolRadius)continue;}
   int cell=next.y*mapWidth+next.x;const auto*nextObject=at[cell];int nextMode=t->terrain==Terrain::Water?1:0;
   if(mode==0&&nextMode==1&&(!nextObject||nextObject->type!=ObjectType::Boat))continue;
   if(mode==1&&nextMode==0&&!(t->originalFlags&64))continue;
   if(!safe(guards[cell])||(nextObject&&nextObject->type==ObjectType::Gate&&!questReady(*nextObject)))continue;
   if(nextObject&&nextObject->type==ObjectType::EnemyHero&&nextObject->owner==actingOwner)continue;
   if(nextObject&&(nextObject->type==ObjectType::Town||nextObject->type==ObjectType::EnemyHero||locationBlocksPassage(*nextObject))&&nextObject->owner!=actingOwner&&!safe(nextObject))continue;
   int state2=nextMode*n+cell,cost=c+moveCost(p,next)+(nextMode!=mode?1500:0);if(cost<dist[state2]){dist[state2]=cost;parent[state2]=state;queue.emplace(cost,state2);}
  }
 }
 if(end<0){for(const auto&o:objects)if(o.type==ObjectType::Portal&&o.level==mapLevel&&o.pos==hero.pos&&o.properties.contains("destination")){if(targetId)*targetId=o.id;return {{hero.pos.x,hero.pos.y,100}};}return {};}std::vector<Step>route;for(int state=end;state!=start;state=parent[state]){if(state<0||parent[state]<0)return {};int cell=state%n,previous=parent[state]%n;Point from{previous%mapWidth,previous/mapWidth},to{cell%mapWidth,cell/mapWidth};route.push_back({to.x,to.y,moveCost(from,to)});}std::reverse(route.begin(),route.end());hero.adventureGoal=object;if(targetId)*targetId=object;return route;
}
void Game::computerTurns(){
 if(!imported()||battle||report||result||!hero.levelChoices.empty())return;
 auto humanNotice=adventureVisit_;adventureVisit_.reset();storeHero();int humanHero=activeHeroId,humanTown=activeTownId,humanLevel=mapLevel;auto humanResources=resources;auto humanValue=hero;auto humanTownValue=town;for(size_t i=0;i<tiles.size();++i)playerFog[0][i]=tiles[i].seen;
 if(computerQueue.empty()){
  for(int owner=1;owner<8;++owner){bool alive=false;for(const auto&[id,h]:heroes)alive|=h.owner==owner;if(alive)continue;actingOwner=owner;activeHeroId=0;activeTownId=0;hero=Hero{};resources=playerResources[owner];for(const auto&o:objects)if(o.type==ObjectType::Town&&o.owner==owner&&settlements.contains(o.id)&&settlements[o.id].built[Tavern]){selectTown(o.id);recruitHero();break;}playerResources[owner]=resources;}
  actingOwner=0;activeHeroId=humanHero;hero=humanValue;activeTownId=humanTown;town=humanTownValue;resources=humanResources;mapLevel=humanLevel;
  for(auto&[id,h]:heroes)if(h.owner>0)computerQueue.push_back(id);
 }
 int attacker=0,defender=0,defendTown=0;
 while(!computerQueue.empty()){
  int id=computerQueue.front();computerQueue.erase(computerQueue.begin());if(!heroes.contains(id))continue;auto record=heroes[id];actingOwner=record.owner;if(!actingOwner)continue;hero=record.value;activeHeroId=id;mapLevel=record.level;resources=playerResources[actingOwner];activeTownId=0;town=Town{};town.pos=hero.pos;for(auto&o:objects)if(o.type==ObjectType::Town&&o.owner==actingOwner){selectTown(o.id);break;}for(size_t i=0;i<tiles.size();++i)tiles[i].seen=playerFog[actingOwner][i];syncHeroObjects();reveal();manageKingdom(true);
  int actions=0,wins=0,encounters=0;Point began=hero.pos;int beganMana=hero.mana,beganMovement=hero.movement;while(hero.movement>0&&actions++<60&&heroes.contains(id)){
   int target=0;auto route=adventurePlan(&target);if(route.empty()||route.front().cost>hero.movement)break;auto*object=objectById(target);
   if(object&&object->owner==0&&(object->type==ObjectType::Town||object->type==ObjectType::EnemyHero)&&route.size()==1){attacker=id;if(object->type==ObjectType::Town){for(const auto&[hid,h]:heroes)if(h.owner==0&&h.level==object->level&&h.value.pos==object->pos){defender=hid;break;}if(!defender)defendTown=object->id;}else defender=object->id;break;}
   auto next=route.front();auto previous=hero.pos;int previousMovement=hero.movement;move({next.x,next.y});
   auto visitSite=[&]{if(auto*site=objectById(target);site&&extendedLocation(site->sourceClass)&&canVisitLocation(*site))autoVisitLocation(target);};visitSite();
   while(battle){++encounters;int turns=0;while(battle&&turns++<3000)ai();if(battle){battle.reset();break;}if(report&&report->won)++wins;report.reset();if(result==-1){result=0;heroes.erase(id);break;}if(!heroes.contains(id))break;while(!hero.levelChoices.empty())chooseSkill(hero.levelChoices.front().skills.front());visitSite();}
   while(!hero.levelChoices.empty())chooseSkill(hero.levelChoices.front().skills.front());takeAdventureVisit();if(hero.pos==previous&&hero.movement==previousMovement)break;manageKingdom(false);
  }
  auto&activity=scenarioRules["computerActivity"];if(!activity.is_array())activity=nlohmann::json::array();activity.push_back({{"day",day},{"owner",actingOwner},{"hero",id},{"from",{began.x,began.y}},{"to",{hero.pos.x,hero.pos.y}},{"actions",actions},{"movementSpent",beganMovement-hero.movement},{"manaSpent",std::max(0,beganMana-hero.mana)},{"encounters",encounters},{"wins",wins}});if(activity.size()>4000)activity.erase(activity.begin());
  if(heroes.contains(id))storeHero();playerResources[actingOwner]=resources;for(size_t i=0;i<tiles.size();++i)playerFog[actingOwner][i]=tiles[i].seen;
  if(attacker)break;
 }
 adventureVisit_=humanNotice;actingOwner=0;activeHeroId=humanHero;hero=humanValue;mapLevel=humanLevel;resources=humanResources;activeTownId=humanTown;town=humanTownValue;for(size_t i=0;i<tiles.size();++i)tiles[i].seen=playerFog[0][i];syncHeroObjects();if(activeTownId&&(!objectById(activeTownId)||objectById(activeTownId)->owner!=0)){activeTownId=0;for(const auto&o:objects)if(o.type==ObjectType::Town&&o.owner==0){selectTown(o.id);break;}}
 if(attacker&&heroes.contains(attacker)){
  if(defender&&heroes.contains(defender)){hero.levelChoices.clear();selectHero(defender);int city=0;for(const auto&o:objects)if(o.type==ObjectType::Town&&o.owner==0&&o.level==mapLevel&&o.pos==hero.pos){city=o.id;break;}startBattle(attacker,0,0,city);}
  else if(defendTown&&settlements.contains(defendTown)){auto&garrison=defendTown==activeTownId?town.garrison:settlements[defendTown].garrison;if(armyCount(garrison)){hero=Hero{};hero.name="城镇守军";hero.attack=hero.defense=hero.power=hero.knowledge=hero.mana=0;hero.army=garrison;hero.pos=objectById(defendTown)->pos;mapLevel=objectById(defendTown)->level;activeHeroId=0;syncHeroObjects();startBattle(attacker,defendTown,humanHero);}else{objectById(defendTown)->owner=heroes[attacker].owner;heroes[attacker].value.pos=objectById(defendTown)->pos;message("一座城镇被电脑占领。");syncHeroObjects();}}
 }
 if(!battle&&computerQueue.empty())checkScenarioVictory();
}
}
