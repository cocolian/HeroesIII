#include "h3/renderer.hpp"
#include <algorithm>

namespace h3 {
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
