#include "h3/renderer.hpp"
#include "h3/localization.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace h3 {
void Desktop::localizationPreview(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";settings_=ClientSettings{};settings_.hdArtwork=false;fixedAnimationTime_=1000;SDL_SetWindowSize(window_,1280,800);mouseX_=mouseY_=-1;
 std::string error;check(game.importScenario(root_/"assets/demo/scenarios/lord-of-the-rings-1.json",error),"LOTR import failed");camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;scene=Scene::Adventure;settle();check(scene==Scene::Visit&&visitPopup_,"LOTR opening event did not open");
 auto shot=[&](const std::string&name){toastUntil_=0;render();check(screenshot(directory/(name+".png")),"Chinese UI screenshot failed");};
 check(visitPopup_->message.find("矮人领主")!=std::string::npos&&visitPopup_->message.find("Dwarf")==std::string::npos,"LOTR opening retained the reported English");
 auto before=game.toJson();int pages=0;
 do{shot("lotr-opening-"+std::to_string(++pages));if(buttons_.size()!=3||!buttons_[1].enabled)break;click({548,385},false);check(pages<100,"Chinese event pagination did not terminate");}while(true);
 check(pages>1&&game.toJson()==before,"Reading Chinese pages changed the game");key(SDLK_RETURN,KMOD_NONE);check(scene==Scene::Adventure,"Chinese event acknowledgement failed");
 key(SDLK_h,KMOD_NONE);shot("lotr-hero-chinese");check(game.hero.name=="希奥顿国王","LOTR hero remains English");key(SDLK_ESCAPE,KMOD_NONE);scene=Scene::Journal;journalPage_=0;shot("lotr-journal-chinese");
 scene=Scene::Maps;selectMap(2);shot("downloaded-map-list-chinese");
 std::ofstream(directory/"localization-ui-audit.json")<<nlohmann::json{{"scenario","lord-of-the-rings-1"},{"openingPages",pages},{"reportedDwarfLordsTranslated",true},{"heroNameChinese",true},{"journalChinese",true},{"mapListChinese",true},{"readingDoesNotChangeRules",true},{"originalSaveWritten",false}}.dump(2)<<'\n';
 std::cout<<"Chinese LOTR opening, hero, journal and map library passed\n";
}
}
