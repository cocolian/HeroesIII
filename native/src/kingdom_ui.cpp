#include "h3/renderer.hpp"
#include <algorithm>

namespace h3 {
void Desktop::neutralDialog(){
 adventure();buttons_.clear();if(!game.neutralOffer){scene=Scene::Adventure;return;}auto offer=*game.neutralOffer;if(neutralOfferId_!=offer.objectId){neutralOfferId_=offer.objectId;neutralReplaceSlot_=-1;}bool full=armyStacks(game.hero.army)==ArmySlots&&std::none_of(game.hero.army.begin(),game.hero.army.end(),[&](auto&s){return s.unit==offer.unit&&s.count<=MaxArmyStack-offer.count;});bool fleeing=offer.intent==NeutralIntent::Flee;
 mapDialog({150,95,500,390});centerText(fleeing?"野兵逃跑":"野兵请求加入",{170,117,460,32},24,{236,213,130,255});portrait(offer.unit,{350,167,100,100});centerText(Units[offer.unit].name+" × "+std::to_string(offer.count),{180,274,440,30},18);
 std::string detail=fleeing?"他们畏惧你的军力。放他们离开，还是追击？":offer.intent==NeutralIntent::JoinFree?"他们愿意免费加入你的军队。":std::string("招募费用：")+std::to_string(offer.cost)+" 金币。";text(detail,190,319,15,{236,213,130,255},420);
 if(full&&!fleeing){for(int i=0;i<ArmySlots;++i){auto unit=game.hero.army[i];SDL_Rect r{167+i*67,352,56,48};portrait(unit.unit,r);text(std::to_string(unit.count),r.x+3,r.y+32,10);if(neutralReplaceSlot_==i)outline(r,{236,213,130,255});button(r,"",[this,i]{neutralReplaceSlot_=i;},true,true,"选择要遣散的部队："+Units[unit.unit].name+" × "+std::to_string(unit.count));}}
 auto answer=[this](bool yes){say(game.answerNeutral(yes,yes?neutralReplaceSlot_:-1));scene=Scene::Adventure;settle();focus_=-1;persist();};
 button({206,410,170,42},fleeing?"放他们离开":full?"替换并加入":"接受加入",[answer]{answer(true);},fleeing||(game.resources[Gold]>=offer.cost&&(!full||neutralReplaceSlot_>=0)),false,"确认当前交涉；招募会检查金币和七格部队容量");button({424,410,170,42},fleeing?"追击":"拒绝",[answer]{answer(false);},true,false,"拒绝加入或追击逃跑的野兵（Esc）");
}
void Desktop::activateHero(int id){if(!game.selectHero(id)){say("请先完成当前战斗、战报或升级选择。");return;}route_.clear();destination_.reset();marching_=false;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;focus_=-1;persist();}
void Desktop::tavern(){
 title("酒馆 · 招募英雄",Scene::Town);text("每位英雄 2500 金币；冒险地图最多八位英雄。",55,88,16);text("新英雄在当前城镇门口加入，请先将门口的英雄移开。",55,120,13);int row=0;for(int id:game.ownedHeroes()){const auto&h=game.heroes.at(id);text(h.value.name+"  等级 "+std::to_string(id==game.activeHeroId?game.hero.level:h.value.level),70,170+row++*30,14);}button({70,454,290,46},"招募一名英雄 · 2500",[this]{say(game.recruitHero());persist();},game.hasTown()&&game.resources[Gold]>=2500&&game.ownedHeroes().size()<8,false,"支付 2500 金币，从当前城镇酒馆招募一名本阵营英雄；城门口需要空位");button({400,454,270,46},"返回城镇",[this]{scene=Scene::Town;},true,false,"关闭酒馆，返回城镇");
}
void Desktop::journal(){
 title("地图剧情与任务记录",Scene::Adventure);const auto&j=game.scenarioRules["journal"];int count=j.is_array()?int(j.size()):0;int pages=std::max(1,(count+7)/8);journalPage_=std::clamp(journalPage_,0,pages-1);if(!count)text("尚未触发地图剧情事件。",55,100,17);for(int row=0;row<8;++row){int index=count-1-journalPage_*8-row;if(index<0)break;const auto&e=j[index];button({48,83+row*48,700,40},"第 "+std::to_string(e.value("day",1))+" 天 · "+e.value("hero",std::string{})+" · "+e.value("title",std::string("事件")),[this,index]{const auto&e=game.scenarioRules["journal"][index];visitPopup_=AdventureVisit{ObjectType::Site,e.value("title",std::string("地图事件")),e.value("message",std::string{})};visitPage_=0;visitMessage_.clear();scene=Scene::Visit;focus_=-1;},true,false,"阅读这条已触发的剧情和任务记录");}button({85,510,150,35},"上一页",[this]{--journalPage_;},journalPage_>0);button({560,510,150,35},"下一页",[this]{++journalPage_;},journalPage_+1<pages);text(std::to_string(journalPage_+1)+" / "+std::to_string(pages),370,519,13);
}
void Desktop::meeting(){
 auto at=game.heroes.find(meetingHero_);if(at==game.heroes.end()){scene=Scene::Adventure;return;}title("英雄会面 · 交换部队",Scene::Adventure);text(game.hero.name,60,95,19);text(at->second.value.name,440,95,19);text("先选左侧部队，再选右侧接收栏。",65,141,13);for(int slot=0;slot<7;++slot){int y=185+slot*42;auto s=game.hero.army[slot],t=at->second.value.army[slot];button({60,y,310,35},s.empty()?"空栏位":Units[s.unit].name+" × "+std::to_string(s.count),[this,slot]{sourceSlot_=slot;},!s.empty(),false,"选择要交给另一位英雄的部队");button({435,y,310,35},t.empty()?"空栏位":Units[t.unit].name+" × "+std::to_string(t.count),[this,slot]{if(sourceSlot_>=0){auto&s=game.hero.army[sourceSlot_];int amount=s.count;if(armyCount(game.hero.army)==amount)--amount;say(game.transferHeroArmy(meetingHero_,sourceSlot_,slot,amount));sourceSlot_=-1;persist();}},sourceSlot_>=0,false,"接收选中的部队；英雄保留最后一名士兵");}button({270,515,265,35},"交换左右英雄",[this]{int previous=game.activeHeroId;if(game.selectHero(meetingHero_)){meetingHero_=previous;sourceSlot_=-1;}},true,false,"交换左右两位英雄，再进行反向部队转移");
}
}
namespace h3 {
void Desktop::townServices(){
 town();buttons_.clear();mapDialog({90,25,620,550},Scene::Town);centerText(game.townDef().buildings[townServiceBuilding_].name,{110,45,580,32},24,{236,213,130,255});
 auto offers=game.townOffers(townServiceBuilding_);int pages=std::max(1,(int(offers.size())+5)/6);locationPage_=std::clamp(locationPage_,0,pages-1);
 if(offers.empty())text(game.visiting()?"目前没有可执行的交易。":"英雄需要进入城镇才能使用此建筑。",130,140,17,{236,213,130,255},540);
 for(int row=0;row<6;++row){int index=locationPage_*6+row;if(index>=int(offers.size()))break;auto f=offers[index];int y=102+row*59;panel({118,y,564,54});if(f.unit>=0)portrait(f.unit,{126,y+5,44,44});else if(f.artifact>=0)frame("artifact.def",0,f.artifact,{126,y+5,44,44});text(f.label,180,y+5,14,{236,213,130,255},350);std::string detail=f.description;for(int r=0;r<ResourceCount;++r)if(f.cost[r])detail+=" "+std::to_string(f.cost[r])+ResourceNames[r];text(detail,180,y+29,10,{190,173,139,255},360);
  button({555,y+8,112,35},f.maximum>1?"全部 · "+std::to_string(f.maximum):"确认",[this,f]{say(game.townService(townServiceBuilding_,f.action,f.parameter,f.maximum));persist();},f.enabled,false,f.label+"："+detail+(f.maximum>1?"；按当前可用数量执行，可先整理部队调整数量":""));
 }
 button({120,477,100,30},"上一页",[this]{--locationPage_;},locationPage_>0);button({578,477,100,30},"下一页",[this]{++locationPage_;},locationPage_+1<pages);centerText(std::to_string(locationPage_+1)+" / "+std::to_string(pages),{280,477,240,30},13);
 originalButton({615,522,64,32},"iokay.def",0,[this]{scene=Scene::Town;focus_=-1;},true,-1,"关闭建筑服务，返回城镇（Esc）");
}
void Desktop::kingdomOverview(){
 originalImage("overview.pcx",{0,0,800,600});centerText(overviewTowns_?"王国总览 · 城镇":"王国总览 · 英雄",{22,2,703,22},17,{236,213,130,255});
 std::vector<int>ids;if(overviewTowns_){for(const auto&o:game.objects)if(o.type==ObjectType::Town&&o.owner==game.currentOwner())ids.push_back(o.id);}else ids=game.ownedHeroes();int pages=std::max(1,(int(ids.size())+3)/4);overviewPage_=std::clamp(overviewPage_,0,pages-1);
 for(int row=0;row<4;++row){int index=overviewPage_*4+row;if(index>=int(ids.size()))break;int id=ids[index],y=30+row*116;Army army{};
  if(overviewTowns_){const auto&t=id==game.activeTownId?game.town:game.settlements.at(id);const auto*o=game.objectById(id);frame("itpa.def",0,2+t.faction*2+(t.built[Castle]?18:0),{31,y+13,48,32});text(o?o->name:"城镇",92,y+7,16);text(Factions[t.faction].name+" · 日收入 "+std::to_string(t.built[Capitol]?4000:t.built[CityHall]?2000:t.built[TownHall]?1000:500),92,y+35,11);army=t.garrison;button({31,y+7,250,95},"",[this,id]{game.selectTown(id);scene=Scene::Town;},true,true,"查看城镇建设、驻军与招募");}
  else{const auto&h=id==game.activeHeroId?game.hero:game.heroes.at(id).value;heroPortrait(h,{31,y+10,58,64});text(h.name+" · 等级 "+std::to_string(h.level),100,y+7,15);text("攻 "+std::to_string(h.attack)+"  防 "+std::to_string(h.defense)+"  力 "+std::to_string(h.power)+"  知 "+std::to_string(h.knowledge),100,y+33,11);text("行动力 "+std::to_string(h.movement)+"  魔法 "+std::to_string(h.mana),100,y+56,11);army=h.army;button({31,y+7,250,95},"",[this,id]{activateHero(id);scene=Scene::Hero;},true,true,"选择英雄并查看属性、技能与装备");}
  for(int slot=0;slot<ArmySlots;++slot)if(!army[slot].empty()){int x=294+slot*60;portrait(army[slot].unit,{x,y+12,48,53});text(std::to_string(army[slot].count),x+3,y+68,12);}
 }
 button({24,499,85,40},"上一页",[this]{--overviewPage_;},overviewPage_>0);button({628,499,85,40},"下一页",[this]{++overviewPage_;},overviewPage_+1<pages);centerText(std::to_string(overviewPage_+1)+" / "+std::to_string(pages),{260,499,220,40},15);
 button({747,493,51,32},"英雄",[this]{overviewTowns_=false;overviewPage_=0;});button({747,530,51,32},"城镇",[this]{overviewTowns_=true;overviewPage_=0;});originalButton({747,568,51,30},"hsbtns.def",0,[this]{scene=Scene::Adventure;focus_=-1;},true,-1,"关闭王国总览（Esc）");resourceBar(578,744);
}
void Desktop::battleExitDialog(){
 battle();buttons_.clear();mapDialog({170,150,460,280},Scene::Battle);bool surrender=battleExitSurrender_;centerText(surrender?"投降":"撤退",{190,170,420,35},25,{236,213,130,255});
 text(surrender?"支付 "+std::to_string(game.surrenderCost())+" 金币，保留幸存部队、经验和宝物。":"撤退将丢失剩余部队，英雄保留经验和宝物。",205,230,16,{236,213,130,255},390);text("英雄可在本周酒馆花费 2500 金币重新招募。",205,290,13,{190,173,139,255},390);
 button({220,354,150,40},"取消",[this]{scene=Scene::Battle;},true,false,"取消，不支付费用或结束战斗（Esc）");button({430,354,150,40},"确认",[this,surrender]{battleAction([this,surrender]{return game.leaveBattle(surrender);});scene=game.battle?Scene::Battle:Scene::Report;persist();settle();},game.canLeaveBattle(surrender)&&(!surrender||game.resources[Gold]>=game.surrenderCost()),false,"确认本次撤退或投降");
}
}

namespace h3 {
Army& Desktop::garrisonArmy(){if((scene==Scene::Army||scene==Scene::Split)&&garrisonObject_){auto*o=game.objectById(garrisonObject_);if(o&&o->owner==game.currentOwner()&&o->level==game.mapLevel&&o->visitableAt(game.hero.pos))return o->army;garrisonObject_=0;}return game.town.garrison;}
std::string Desktop::transferArmy(bool a,int from,bool b,int to,int n){return (scene==Scene::Army||scene==Scene::Split)&&garrisonObject_?game.transferGarrison(garrisonObject_,a,from,b,to,n):game.transfer(a,from,b,to,n);}
}
namespace h3 {
void Desktop::necromancyDialog(){
 if(!game.report||!game.report->raisedPending){afterReport();return;}const auto raised=game.report->raised;mapDialog({90,95,620,410});centerText("招魂术 · 部队已满",{110,112,580,35},24,{236,213,130,255});portrait(raised.unit,{366,160,68,74});centerText(Units[raised.unit].name+" × "+std::to_string(raised.count),{110,236,580,25},17);
 button({150,440,180,36},"放弃本次招魂",[this]{say(game.answerNecromancy(-1));game.report.reset();afterReport();persist();},true,false,"放弃新复生的部队，保留现有军队（Enter / Esc）");
 for(int slot=0;slot<ArmySlots;++slot){auto s=game.hero.army[slot];SDL_Rect rect{112+slot*83,282,72,82};panel(rect);if(!s.empty()){portrait(s.unit,{rect.x+7,rect.y+3,58,64});text(std::to_string(s.count),rect.x+7,rect.y+62,11);}if(necromancySlot_==slot)outline(rect,{236,213,130,255});button(rect,"",[this,slot]{necromancySlot_=slot;},true,true,s.empty()?"空栏位":std::string("选择要遣散的整支部队：")+Units[s.unit].name+" × "+std::to_string(s.count));}
 std::string textMessage="选择一支部队后确认替换，或放弃本次招魂。";if(necromancySlot_>=0){const auto&s=game.hero.army[necromancySlot_];textMessage=s.empty()?"将复生部队加入空栏位。":"将遣散 "+Units[s.unit].name+" × "+std::to_string(s.count)+"，接收招魂部队。";}text(textMessage,118,383,14,{236,213,130,255},560);
 button({460,440,180,36},"确认替换",[this]{say(game.answerNecromancy(necromancySlot_));game.report.reset();necromancySlot_=-1;afterReport();persist();},necromancySlot_>=0,false,"确认遣散选中的整支部队，并接收招魂所得士兵");
}
}
