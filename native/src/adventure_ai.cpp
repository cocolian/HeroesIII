#include "h3/game.hpp"
#include "h3/locations.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <tuple>

namespace h3 {
std::vector<Step> Game::adventurePlan(int* targetId) {
 if(targetId)*targetId=0;
 if(battle||report||result||hero.movement<=0)return {};
 const int plane=mapWidth*mapHeight,total=plane*mapLevels;
 auto index=[&](Point p,int z){return z*plane+p.y*mapWidth+p.x;};
 auto ground=[&](Point p,int z)->const Tile*{
  if(z<0||z>=mapLevels||p.x<0||p.x>=mapWidth||p.y<0||p.y>=mapHeight)return nullptr;
  return &tiles[index(p,z)];
 };
 std::vector<const Object*> at(total),guards(total);
 const double own=armyPower(hero.army)*(1.+(primarySkill(0)+primarySkill(1))/30.);
 auto remembered=[&](int id,double actual){
  if(scenarioRules.is_object()&&scenarioRules.contains("observedThreatStrengths")){
   const auto& known=scenarioRules["observedThreatStrengths"];auto key=std::to_string(id);
   if(known.is_object()&&known.contains(key)&&known[key].is_number())actual=std::max(actual,known[key].get<double>());
  }return actual;
 };
 std::map<int,bool> safety;
 auto safe=[&](const Object* o){
  if(!o)return true;
  if(auto it=safety.find(o->id);it!=safety.end())return it->second;
  double strength=locations::estimatedGuards(*o);
  if(heroes.contains(o->id)){const auto& h=heroes.at(o->id).value;strength*=1.+(heroPrimary(h,0)+heroPrimary(h,1))/30.;}
  if(o->type==ObjectType::Town&&o->owner!=actingOwner&&settlements.contains(o->id)){
   const auto& t=settlements.at(o->id);
   if(t.built[Fort]||t.built[Citadel]||t.built[Castle])strength*=1.35;
   if(t.built[Citadel]||t.built[Castle])strength+=700*(t.built[Castle]?2:1);
  }
  return safety[o->id]=remembered(o->id,strength)*1.65<own;
 };
 for(const auto& o:objects)if(auto* t=ground(o.pos,o.level);t&&t->seen){
  int i=index(o.pos,o.level);
  if(o.type!=ObjectType::MapEvent&&(!at[i]||(at[i]->type!=ObjectType::EnemyHero&&(o.type==ObjectType::EnemyHero||o.type==ObjectType::Town))))at[i]=&o;
  if(o.type==ObjectType::Monster&&o.owner!=actingOwner&&armyCount(o.army))for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
   Point p{o.pos.x+dx,o.pos.y+dy};auto* g=ground(p,o.level);
   if(!g||(g->terrain==Terrain::Water)!=(t->terrain==Terrain::Water))continue;
   auto& old=guards[index(p,o.level)];if(old&&old->pos==p)continue;
   if(!old||o.pos==p||std::tie(o.pos.x,o.pos.y,o.id)<std::tie(old->pos.x,old->pos.y,old->id))old=&o;
  }
 }
 struct Threat{Point pos;int level;};std::vector<Threat> threats;
 for(const auto& o:objects)if(o.type==ObjectType::EnemyHero&&o.owner!=actingOwner){
  auto* t=ground(o.pos,o.level);if(!t||!t->seen)continue;
  double strength=armyPower(o.army);if(heroes.contains(o.id)){const auto& h=heroes.at(o.id).value;strength*=1.+(heroPrimary(h,0)+heroPrimary(h,1))/30.;}
  if(remembered(o.id,strength)>own*1.1)threats.push_back({o.pos,o.level});
 }
 auto range=[](Point a,Point b){return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y));};
 auto dangerDistance=[&](Point p,int z){int nearest=1000;for(auto enemy:threats)if(enemy.level==z)nearest=std::min(nearest,range(p,enemy.pos));return nearest;};
 const int initialDanger=dangerDistance(hero.pos,mapLevel);const bool escaping=initialDanger<23;
 const int commander=armyLeader();const Hero* recipient=nullptr;int recipientLevel=-1;
 int keeper=-1;for(int slot=0;slot<ArmySlots;++slot)if(!hero.army[slot].empty()&&(keeper<0||Units[hero.army[slot].unit].speed>Units[hero.army[keeper].unit].speed))keeper=slot;
 auto usefulArmy=[&](const Army& offered,const Hero& destination,int keep){
  int reserved=destination.skills[Necromancy]?baseUnit(raisedUnit(destination)):-1;
  double gain=0;for(int slot=0;slot<ArmySlots;++slot){auto s=offered[slot];s.count-=slot==keep?1:0;if(s.empty())continue;
   Army a{};a[0]=s;double power=armyPower(a),best=0;
   for(const auto& unit:destination.army){if(unit.empty()||unit.unit==s.unit)best=std::max(best,power);else if(baseUnit(unit.unit)!=reserved){Army b{};b[0]=unit;double old=armyPower(b);if(power>old*1.3)best=std::max(best,power-old);}}
   gain+=best;
  }return gain;
 };
 if(armyCount(hero.army)>1&&commander!=activeHeroId&&heroes.contains(commander)){
  const auto& h=heroes.at(commander);
  if(usefulArmy(hero.army,h.value,keeper)>std::max(100.,armyPower(h.value.army)*.03)){recipient=&h.value;recipientLevel=h.level;}
 }
 auto stepCost=[&](Point from,Point to,int z){
  const auto* t=ground(to,z);const auto* f=ground(from,z);if(!t||!f)return 999999;
  int penalty=t->terrain==Terrain::Sand||t->terrain==Terrain::Snow?50:t->terrain==Terrain::Swamp?75:t->terrain==Terrain::Rough?25:0;
  const Terrain native[]={Terrain::Grass,Terrain::Grass,Terrain::Snow,Terrain::Lava,Terrain::Dirt,Terrain::Subterranean,Terrain::Rough,Terrain::Swamp,Terrain::Grass};
  bool nativeArmy=armyCount(hero.army)>0;for(const auto& s:hero.army)if(!s.empty())nativeArmy&=Units[s.unit].faction>=0&&native[Units[s.unit].faction]==t->terrain;
  if(nativeArmy||(t->terrain==Terrain::Sand&&std::any_of(hero.army.begin(),hero.army.end(),[](auto&s){return !s.empty()&&(creatureAbility(s.unit).flags&512);})))penalty=0;penalty=std::max(0,penalty-std::array<int,4>{0,25,50,100}[hero.skills[Pathfinding]]);
  int cost=f->road&&t->road?75:100+penalty;return from.x!=to.x&&from.y!=to.y?int(std::floor(cost*std::sqrt(2.))):cost;
 };
 auto knownPortal=[&](const Object& o){return o.properties.is_object()&&(o.properties.value("portalKnownOwners",0)&(1<<actingOwner));};
 auto patrolAllows=[&](Point p,int z){
  if(actingOwner>0&&heroes.contains(activeHeroId)){const auto& h=heroes.at(activeHeroId);if(h.patrolRadius>=0)return z==h.level&&range(p,h.patrolOrigin)<=h.patrolRadius;}return true;
 };
 constexpr int Inf=100000000;std::vector<int> dist(total*2,Inf),parent(total*2,-1);std::vector<bool> portalEdge(total*2);
 using Node=std::pair<int,int>;std::priority_queue<Node,std::vector<Node>,std::greater<Node>> queue;
 int start=(hero.onBoat?total:0)+index(hero.pos,mapLevel);dist[start]=0;queue.emplace(0,start);
 double best=0;int end=-1,object=0;
 auto relax=[&](int from,int to,int cost,bool portal=false){if(cost<dist[to]){dist[to]=cost;parent[to]=from;portalEdge[to]=portal;queue.emplace(cost,to);}};
 while(!queue.empty()){
  auto[c,state]=queue.top();queue.pop();if(c!=dist[state])continue;
  int i=state%total,z=i/plane,cell=i%plane,mode=state/total;Point p{cell%mapWidth,cell/mapWidth};
  const auto* o=at[i];const auto* guard=guards[i];
  bool enemy=o&&(o->type==ObjectType::Town||o->type==ObjectType::EnemyHero)&&o->owner!=actingOwner;
  bool encounter=guard||(o&&o->owner!=actingOwner&&armyCount(o->army)>0)||(o&&locationBlocksPassage(*o));
  if(state!=start){
   double value=0;int target=0;
   if(guard){if(safe(guard)){value=180;target=guard->id;}}
   else if(o){target=o->id;switch(o->type){
    case ObjectType::Town:
     if(enemy&&safe(o))value=1600;
     else if(o->owner==actingOwner){auto t=settlements.find(o->id);if(t!=settlements.end()){
      const auto& city=o->id==activeTownId?town:t->second;
      double useful=usefulArmy(city.garrison,hero,-1);if(useful>std::max(500.,own*.2))value=900+std::min(2000.,useful*.02);
     }}break;
    case ObjectType::EnemyHero:if(enemy&&!sanctuaryProtected(o->id)&&safe(o))value=800;break;
    case ObjectType::Mine:if(o->owner!=actingOwner&&safe(o))value=550;break;
    case ObjectType::Treasure:case ObjectType::Resource:if(safe(o)&&!(resources[Gold]>50000&&own>5000))value=200;break;
    case ObjectType::Artifact:if(safe(o))value=300;break;
    case ObjectType::Portal:if(o->properties.contains("destination")&&!knownPortal(*o))value=600;break;
    case ObjectType::Gate:if(questReady(*o))value=500;break;
    case ObjectType::Windmill:case ObjectType::WaterWheel:if(!o->visited&&resources[Gold]<50000)value=160;break;
    case ObjectType::Well:if(hero.mana<maxMana()/2&&hero.wellDay!=day)value=100;break;
    case ObjectType::Site:
     if((o->sourceClass==33||o->sourceClass==219||o->sourceClass==6)&&o->owner!=actingOwner&&safe(o))value=600;
     else if(bankLocation(o->sourceClass)){
      auto offers=locationOffers(o->id);bool cleared=o->properties.is_object()&&o->properties.value("locationCleared",false),ready=o->properties.is_object()&&o->properties.value("locationBankReady",false);if(!cleared&&safe(o)&&(!ready||armyCount(o->army)>0||std::any_of(offers.begin(),offers.end(),[](const auto& f){return f.enabled;})))value=500;
     }else if(dwellingLocation(o->sourceClass)&&safe(o)){
      auto offers=locationOffers(o->id);if(o->sourceClass!=78&&o->owner!=actingOwner)value=400;
      for(const auto& f:offers)if(f.action==LocationAction::Hire&&f.enabled){Army recruits{};recruits[0]={f.unit,f.maximum};value=std::max(value,std::min(800.,400*armyPower(recruits)/std::max(500.,own*.25)));}
     }else if(extendedLocation(o->sourceClass)&&!o->visited&&std::find(hero.aiVisitedSites.begin(),hero.aiVisitedSites.end(),o->id)==hero.aiVisitedSites.end()&&o->sourceClass!=27&&o->sourceClass!=2&&o->sourceClass!=50&&o->sourceClass!=80&&safe(o))value=150;
     else if(o->sourceClass==42&&o->owner!=actingOwner&&safe(o))value=350;
     else if(!o->visited&&(o->sourceClass==12||o->sourceClass==23||o->sourceClass==32||o->sourceClass==51||o->sourceClass==61||o->sourceClass==100))value=180;
     break;
    default:break;
   }}
   if(!encounter&&(!o||o->type!=ObjectType::Gate)){
    int unknown=0;for(int dy=-5;dy<=5;dy+=2)for(int dx=-5;dx<=5;dx+=2)if(dx*dx+dy*dy<30)if(auto* t=ground({p.x+dx,p.y+dy},z);t&&!t->seen&&t->terrain!=Terrain::Rock)++unknown;
    value+=unknown*18;
   }
   double score=value/(c+450.);
   if(recipient&&z==recipientLevel&&range(p,recipient->pos)<=1)score+=20000;
   if(target&&target==hero.adventureGoal&&value>0)score+=10000;
   if(escaping){score=0;if(c<=hero.movement&&!encounter&&dangerDistance(p,z)>initialDanger)score=100000.+1000.*dangerDistance(p,z)-c*.01;}
   if(score>best){best=score;end=state;object=escaping?0:target;}
  }
  // Encounter edges are strategic estimates. The executable route below ends
  // at the first actual encounter, so no unit can walk through a living guard.
  if(o&&o->type==ObjectType::Portal&&o->properties.contains("destination")&&knownPortal(*o)){
   const auto& d=o->properties["destination"];Point destination{d.value("x",-1),d.value("y",-1)};int dz=d.value("level",-1);
   auto* t=ground(destination,dz);
   bool occupied=std::any_of(heroes.begin(),heroes.end(),[&](const auto& h){return h.first!=activeHeroId&&h.second.level==dz&&h.second.value.pos==destination;});
   if(t&&t->seen&&!t->blocked&&t->decor==Decor::None&&patrolAllows(destination,dz)&&!occupied&&(t->terrain==Terrain::Water)==bool(mode)){
    int j=index(destination,dz);if(safe(guards[j])&&safe(at[j]))relax(state,mode*total+j,c+100,true);
   }
  }
  for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
   if(!dx&&!dy)continue;Point next{p.x+dx,p.y+dy};auto* t=ground(next,z);
   if(!t||!t->seen||t->blocked||t->decor!=Decor::None||t->terrain==Terrain::Rock||!patrolAllows(next,z))continue;
   if(!imported()&&dx&&dy){auto* a=ground({p.x+dx,p.y},z);auto* b=ground({p.x,p.y+dy},z);if(!a||!b||a->blocked||b->blocked||a->decor!=Decor::None||b->decor!=Decor::None)continue;}
   bool threatened=false;for(auto enemy:threats)if(enemy.level==z&&range(next,enemy.pos)<=6&&range(next,enemy.pos)<range(p,enemy.pos)){threatened=true;break;}if(threatened)continue;
   int j=index(next,z),nextMode=t->terrain==Terrain::Water;const auto* nextObject=at[j];
   if(mode==0&&nextMode==1&&(!nextObject||nextObject->type!=ObjectType::Boat))continue;
   if(mode==1&&nextMode==0&&!(t->originalFlags&64))continue;
   if(!safe(guards[j])||(nextObject&&nextObject->type==ObjectType::Gate&&!questReady(*nextObject)))continue;
   if(nextObject&&nextObject->type==ObjectType::EnemyHero&&(nextObject->owner==actingOwner||sanctuaryProtected(nextObject->id)))continue;
   if(nextObject&&nextObject->owner!=actingOwner&&(nextObject->type==ObjectType::Town||nextObject->type==ObjectType::EnemyHero||locationBlocksPassage(*nextObject))&&!safe(nextObject))continue;
   int battleCost=guards[j]&&guards[j]!=guard?400:0;
   if(nextObject&&nextObject!=o&&nextObject->owner!=actingOwner&&armyCount(nextObject->army)>0&&!guards[j])battleCost+=400;
   relax(state,nextMode*total+j,c+stepCost(p,next,z)+(nextMode!=mode?1500:0)+battleCost);
  }
 }
 // Wait only for a supply army which has a known traversable route to us.
 if(commander==activeHeroId&&initialDanger>10)for(const auto& [id,h]:heroes)if(id!=activeHeroId&&h.owner==actingOwner&&h.level==mapLevel&&h.value.onBoat==hero.onBoat){
  int keep=-1;for(int slot=0;slot<ArmySlots;++slot)if(!h.value.army[slot].empty()&&(keep<0||Units[h.value.army[slot].unit].speed>Units[h.value.army[keep].unit].speed))keep=slot;
  if(usefulArmy(h.value.army,hero,keep)<=std::max(500.,armyPower(hero.army)*.2))continue;
  int distance=Inf;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){Point p{h.value.pos.x+dx,h.value.pos.y+dy};if(ground(p,h.level))distance=std::min(distance,dist[(hero.onBoat?total:0)+index(p,h.level)]);}
  if(distance<=3000){hero.adventureGoal=0;return {};}
 }
 if(end<0){
  if(!hero.onBoat&&resources[Gold]>=1000&&resources[Wood]>=10){std::vector<Step> bestRoute;int yard=0;
   for(const auto& o:objects)if(o.sourceClass==87&&o.level==mapLevel&&tile(o.pos)->seen&&std::none_of(objects.begin(),objects.end(),[&](const auto& b){return b.type==ObjectType::Boat&&b.level==o.level&&range(b.pos,o.pos)<=5;})){
    auto route=path(o.pos);if(!route.empty()&&(bestRoute.empty()||route.size()<bestRoute.size())){bestRoute=std::move(route);yard=o.id;}
   }if(yard){hero.adventureGoal=yard;if(targetId)*targetId=yard;return bestRoute;}
  }return {};
 }
 std::vector<int> states;for(int state=end;state!=start;state=parent[state]){if(state<0||parent[state]<0)return {};states.push_back(state);}std::reverse(states.begin(),states.end());
 std::vector<Step> route;int actualTarget=object,previous=start;
 for(int state:states){
  int i=state%total,cell=i%plane,fromCell=previous%plane;Point to{cell%mapWidth,cell/mapWidth},from{fromCell%mapWidth,fromCell/mapWidth};
  if(portalEdge[state]){if(route.empty()){route.push_back({hero.pos.x,hero.pos.y,100});if(at[previous%total])actualTarget=at[previous%total]->id;}break;}
  route.push_back({to.x,to.y,stepCost(from,to,i/plane)});
  const auto* o=at[i];const auto* g=guards[i];
  if(g){actualTarget=g->id;break;}
  if(o&&(o->type==ObjectType::Portal||o->type==ObjectType::Gate||locationBlocksPassage(*o)||(o->owner!=actingOwner&&(o->type==ObjectType::Town||o->type==ObjectType::EnemyHero||armyCount(o->army)>0)))){actualTarget=o->id;break;}
  if(state/total!=previous/total)break;
  previous=state;
 }
 hero.adventureGoal=object;if(targetId)*targetId=actualTarget;return route;
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
   auto next=route.front();auto previous=hero.pos;int previousMovement=hero.movement;move({next.x,next.y});autoNeutral();
   auto visitSite=[&]{if(auto*site=objectById(target);site&&extendedLocation(site->sourceClass)&&canVisitLocation(*site))autoVisitLocation(target);};visitSite();
   while(battle){++encounters;int turns=0;while(battle&&turns++<3000)ai();if(battle){battle.reset();break;}if(report&&report->won)++wins;autoNecromancy();report.reset();if(result==-1){result=0;heroes.erase(id);break;}if(!heroes.contains(id))break;while(!hero.levelChoices.empty())chooseSkill(hero.levelChoices.front().skills.front());visitSite();}
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
 if(!battle){checkScenarioVictory();if(!result&&!computerQueue.empty())computerTurns();}
}
}
