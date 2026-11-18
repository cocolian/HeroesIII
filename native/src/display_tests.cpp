#include "h3/renderer.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace h3 {
void Desktop::displayPreview(const std::filesystem::path& directory){
 std::filesystem::create_directories(directory);saveDir_=directory/"test-saves";
 displayRenderTests(directory);menuRenderTests(directory);mineIncomeRenderTests(directory);terrainRenderTests(directory);adventureArtworkRenderTests(directory);mapArtworkRenderTests(directory);adventureVisitRenderTests(directory);cursorRenderTests(directory);combatCursorRenderTests(directory);outcomeRenderTests(directory);ownershipRenderTests(directory);battleHealthRenderTests(directory);battleTargetRenderTests(directory);touchRenderTests(directory);hintRenderTests(directory);encounterRenderTests(directory);campaignFlowRenderTests(directory);
 std::cout<<"Expanded display and complete demo-map flow passed\n";
}
void Desktop::adventureArtworkRenderTests(const std::filesystem::path& directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int oldW=0,oldH=0;SDL_GetWindowSize(window_,&oldW,&oldH);SDL_SetWindowSize(window_,1280,800);
 game=Game::create();scene=Scene::Adventure;settings_=ClientSettings{};heroDirection_={1,0};
 fixedAnimationTime_=0;mouseX_=mouseY_=-1;marching_=false;route_.clear();destination_.reset();camX_=4.5f;camY_=20.5f;zoom_=1;render();
 click(mapScreen({7,21}),false);check(route_.size()>1,"Adventure art preview has no planned route");
 auto state=game.toJson();nlohmann::json scales=nlohmann::json::array();
 for(int percent:{65,100,180}){
  zoom_=percent/100.f;camX_=game.hero.pos.x+.5f;camY_=game.hero.pos.y+.5f;toastUntil_=0;render();
  auto map=adventureMapRect();auto p=mapScreen(game.hero.pos);SDL_Point heroPoint{p.x,p.y};
  check(SDL_PointInRect(&heroPoint,&map),"Zoom preview placed the hero outside the visible map");
  bool checkedShroud=false;
  for(int y=1;y<MapH-1&&!checkedShroud;++y)for(int x=1;x<MapW-1&&!checkedShroud;++x){
   bool hidden=true;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)hidden=hidden&&!game.tile({x+dx,y+dy})->seen;
   if(!hidden)continue;auto center=mapScreen({x,y});int tile=int(std::lround(32*zoom_));SDL_Rect logical{center.x-tile/2+2,center.y-tile/2+2,tile-4,tile-4};
   if(logical.x<map.x||logical.y<map.y||logical.x+logical.w>map.x+map.w||logical.y+logical.h>map.y+map.h)continue;
   SDL_Rect area{int(logical.x*pixelScale_),int(logical.y*pixelScale_),int(logical.w*pixelScale_),int(logical.h*pixelScale_)};
   std::vector<Uint8> pixels(size_t(area.w)*area.h*4);
   check(SDL_SetRenderTarget(renderer_,canvas_.texture)==0,"Shroud regression canvas unavailable");
   bool read=SDL_RenderReadPixels(renderer_,&area,SDL_PIXELFORMAT_RGBA32,pixels.data(),area.w*4)==0;
   SDL_SetRenderTarget(renderer_,nullptr);check(read,"Shroud regression pixel readback failed");
   int dark=0,stars=0;for(size_t i=0;i<pixels.size();i+=4){dark+=int(pixels[i])+pixels[i+1]+pixels[i+2]<48;stars+=pixels[i+2]>16&&pixels[i+2]>pixels[i]+8;}
   check(dark>area.w*area.h*4/5,"Unexplored terrain leaked through the full shroud");checkedShroud=true;
   if(stars<=area.w*area.h/100)throw std::runtime_error("Full shroud lost stars at zoom "+std::to_string(percent)+" tile "+std::to_string(x)+","+std::to_string(y)+" area "+std::to_string(area.x)+","+std::to_string(area.y)+" stars "+std::to_string(stars)+" / "+std::to_string(area.w*area.h));
  }
  check(checkedShroud,"Zoom preview did not exercise an unexplored tile");
  check(screenshot(directory/("adventure-route-"+std::to_string(percent)+".png")),"Adventure arrow/hero preview failed");
  scales.push_back({{"zoomPercent",percent},{"heroCanvasPoints",{int(std::lround(96*zoom_)),int(std::lround(64*zoom_))}},{"tilePoints",int(std::lround(32*zoom_))}});
 }
 check(game.toJson()==state,"Adventure artwork changed the game state");
 zoom_=1;game.hero.movement=route_.front().cost;toastUntil_=0;render();
 check(screenshot(directory/"adventure-route-next-day.png"),"Next-day arrow preview failed");
 std::ofstream(directory/"adventure-art-audit.json")<<nlohmann::json{{"pathAsset","adag.def"},{"pathReachableColor","original green"},{"pathFutureColor","original red"},{"destination","original cross"},{"heroAsset","ah00_.def"},{"ownerFlagAsset","af01.def"},{"originalHeroCanvas",{96,64}},{"tileCanvas",{32,32}},{"zoomPreviews",scales},{"shroudAsset","tshrc.def"},{"shroudEdgeAsset","tshre.def"},{"unexploredTerrainStaysHiddenAtAllZooms",true},{"gameStateUnchangedByRendering",true}}.dump(2)<<'\n';
 game=Game::create();route_.clear();destination_.reset();heroDirection_={1,0};zoom_=1;SDL_SetWindowSize(window_,oldW,oldH);scene=Scene::Town;
}
void Desktop::displayRenderTests(const std::filesystem::path& directory){
 auto check=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
 int oldW=0,oldH=0;SDL_GetWindowSize(window_,&oldW,&oldH);nlohmann::json sizes=nlohmann::json::array();
 game=Game::create();scene=Scene::Adventure;settings_=ClientSettings{};fixedAnimationTime_=0;toastUntil_=0;
 camX_=17.5f;camY_=14.5f;zoom_=1;route_.clear();destination_.reset();marching_=false;mouseX_=mouseY_=-1;focus_=-1;
 const auto before=game.toJson();
 for(auto size:{Point{800,600},Point{1280,800},Point{1512,982},Point{1920,1080}}){
  SDL_SetWindowSize(window_,size.x,size.y);render();auto map=adventureMapRect();
  check(map.w==size.x-208&&map.h==size.y-56,"Adventure viewport did not expand with window");
  auto a=mapScreen({17,14}),b=mapScreen({18,14});check(b.x-a.x==32,"Wider map enlarged tiles");
  check(windowPoint({a.x,a.y})==a&&mapPoint(a)==Point{17,14},"Expanded window pointer conversion missed tile");
  check(game.toJson()==before,"Resizing changed game rules or map");
  check(screenshot(directory/("display-map-"+std::to_string(size.x)+".png")),"Expanded map screenshot failed");
  sizes.push_back({{"windowPoints",{size.x,size.y}},{"mapPoints",{map.w,map.h}},{"tilePoints",32},{"sidebarPoints",192},{"visibleColumns",map.w/32.},{"visibleRows",map.h/32.}});
 }
 SDL_SetWindowSize(window_,1280,800);scene=Scene::Adventure;camX_=4.5f;camY_=20.5f;render();
 auto pointer=[&](Uint32 type,Point p){SDL_Event e{};e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=p.x;e.button.y=p.y;handleNativeEvent(e,true);};
 auto mini=adventureMinimapRect();Point target{mini.x+mini.w*3/4,mini.y+mini.h/4};
 pointer(SDL_MOUSEBUTTONDOWN,{mini.x+mini.w/2,mini.y+mini.h/2});pointer(SDL_MOUSEBUTTONUP,target);
 check(std::abs(camX_-(MapW-adventureMapRect().w/64.f))<.3f&&std::abs(camY_-adventureMapRect().h/64.f)<.3f,"Minimap release did not pan camera");
 camX_=4.5f;camY_=20.5f;route_.clear();destination_.reset();render();click(mapScreen({7,21}),false);key(SDLK_m,KMOD_NONE);
 auto planned=route_;auto destination=destination_;click(mapScreen({6,20}),false);
 check(!marching_&&destination_==destination&&route_.size()==planned.size(),"Map click while moving did not stop and retain route");
 render();int side=480;click({696+side,212},false);check(scene==Scene::Overview,"Kingdom icon did not open the kingdom overview");key(SDLK_ESCAPE,KMOD_NONE);check(scene==Scene::Adventure,"Kingdom overview did not return to adventure");
 click({728+side,308},false);check(scene==Scene::Options,"Original System Options button is miswired");key(SDLK_ESCAPE,KMOD_NONE);
 key(SDLK_F4,KMOD_NONE);bool entered=(SDL_GetWindowFlags(window_)&SDL_WINDOW_FULLSCREEN_DESKTOP)!=0;
 check(entered,"F4 did not enter desktop fullscreen");render();key(SDLK_F4,KMOD_NONE);check(!(SDL_GetWindowFlags(window_)&SDL_WINDOW_FULLSCREEN_DESKTOP),"F4 did not leave fullscreen");
 SDL_SetWindowSize(window_,1280,800);scene=Scene::Adventure;render();
 SDL_Event reset{};reset.type=SDL_RENDER_TARGETS_RESET;handleEvent(reset);check(!canvas_.texture,"Render-target reset retained invalid canvas");render();check(canvas_.texture,"Render-target reset did not recreate canvas");
 reset.type=SDL_RENDER_DEVICE_RESET;handleEvent(reset);check(!canvas_.texture&&images_.empty()&&texts_.empty(),"Device reset retained renderer textures");render();check(canvas_.texture&&!images_.empty(),"Device reset did not rebuild artwork");
 check(game.toJson()==before,"Display controls changed gameplay");
 scene=Scene::Town;render();auto classic=displayLayout(1280,800,1280,800,false);check(classic.width==800&&classic.height==600,"Town layout lost original aspect ratio");
 check(windowPoint({int(std::lround(classic.pointX+400.5*classic.pointScale)),int(std::lround(classic.pointY+300.5*classic.pointScale))})==Point{400,300},"Classic modal pointer conversion failed after expanded map");
 std::ofstream(directory/"display-layout-audit.json")<<nlohmann::json{{"sizes",sizes},{"desktopFullscreenToggle",entered},{"retinaUsesWindowPoints",true},{"minimapNativeRelease",true},{"mapClickStopsMovement",true},{"rendererResetRebuild",true},{"originalSystemOptionsPosition",true},{"classicDialogsKeepAspect",true}}.dump(2)<<'\n';
 // Separate visual fixtures cover every current Scene; these are not the
 // natural campaign evidence above or a claim that missing original UI exists.
 SDL_SetWindowSize(window_,800,600);
 auto pixels=[&](SDL_Rect area){
  area={int(area.x*pixelScale_),int(area.y*pixelScale_),int(area.w*pixelScale_),int(area.h*pixelScale_)};
  std::vector<Uint32> result(size_t(area.w)*area.h);
  check(SDL_SetRenderTarget(renderer_,canvas_.texture)==0,"Toast regression canvas unavailable");
  bool read=SDL_RenderReadPixels(renderer_,&area,SDL_PIXELFORMAT_RGBA32,result.data(),area.w*4)==0;
  SDL_SetRenderTarget(renderer_,nullptr);check(read,"Toast regression pixel readback failed");return result;
 };
 nlohmann::json toastAudit=nlohmann::json::array();
 const std::pair<Scene,const char*> scenes[]={{Scene::Menu,"menu"},{Scene::Adventure,"adventure"},{Scene::Town,"town"},{Scene::Build,"hall"},{Scene::Recruit,"recruit"},{Scene::Army,"army"},{Scene::Split,"split"},{Scene::Hero,"hero"},{Scene::Spells,"spellbook"},{Scene::Battle,"battle"},{Scene::Level,"level"},{Scene::Report,"report"},{Scene::Help,"help"},{Scene::Codex,"codex"},{Scene::Scenario,"scenario"},{Scene::University,"university"},{Scene::MageGuild,"guild"},{Scene::Market,"market"},{Scene::BuildConfirm,"build-confirm"},{Scene::Fort,"fort"},{Scene::Options,"options"}};
 for(auto [target,name]:scenes){
  battleMotion_.clear();game=Game::create(1999,target==Scene::University?8:0);scene=target;
  camX_=4.5f;camY_=20.5f;zoom_=1;pendingSpell_=sacrificeTarget_=sourceSlot_=-1;mouseX_=mouseY_=-1;focus_=-1;toastUntil_=0;
  route_.clear();destination_.reset();marching_=false;autoCombat_=false;fixedAnimationTime_=0;
  if(target==Scene::Recruit)openRecruit(Pikeman,Scene::Town);
  if(target==Scene::Army)armyReturn_=Scene::Town;
  if(target==Scene::Split){sourceSlot_=0;sourceGarrison_=false;targetSlot_=3;targetGarrison_=false;splitAmount_=14;splitReturn_=Scene::Army;}
  if(target==Scene::Spells||target==Scene::MageGuild){game.build(MageGuild);game.buySpellbook();spellFilter_=spellPage_=0;spellAdventure_=false;}
  if(target==Scene::Battle)game.startBattle(15);
  if(target==Scene::Level)game.gainXP(1000);
  if(target==Scene::Report){game.startBattle(15);int actions=0;while(game.battle&&actions++<500)game.ai();check(game.report.has_value(),"Report visual fixture failed");}
  if(target==Scene::Market){game.build(Marketplace);scene=Scene::Town;openMarket();}
  if(target==Scene::BuildConfirm)buildingPopup(MageGuild,Scene::Build);
  if(target==Scene::Options){scene=Scene::Adventure;openOptions();}
  check(scene==target,"Interface audit fixture did not reach requested Scene");
  render();check(screenshot(directory/(std::string("ui-")+name+".png")),"Interface audit capture failed");
  settings_.hdArtwork=false;render();check(screenshot(directory/(std::string("ui-classic-")+name+".png")),"Classic reference comparison capture failed");settings_.hdArtwork=true;render();
  if(target==Scene::Battle||target==Scene::MageGuild||target==Scene::Report||target==Scene::Level){
   // A transient message must leave combat controls, guild resources, and
   // mandatory result/level dialogs visible. Read the rendered pixels so this
   // catches an opaque overlay even when its buttons still accept clicks.
   SDL_Rect area=target==Scene::Battle?SDL_Rect{0,560,210,40}:target==Scene::Report?SDL_Rect{550,526,64,32}:target==Scene::MageGuild?SDL_Rect{80,579,640,11}:SDL_Rect{80,555,640,35};
   auto unobscured=pixels(area);say("操作提示：界面按钮与资源栏应保持可见。");render();
   check(pixels(area)==unobscured,"Transient message obscured scene controls or footer");
   check(screenshot(directory/(std::string("ui-")+name+"-message.png")),"Toast regression screenshot failed");
   toastAudit.push_back({{"scene",name},{"unobscuredPixels",{area.x,area.y,area.w,area.h}},{"messageActive",true}});
  }
 }
 std::ofstream(directory/"message-layout-audit.json")<<toastAudit.dump(2)<<'\n';
 game=Game::create();sourceSlot_=-1;pendingSpell_=-1;
 SDL_SetWindowSize(window_,oldW,oldH);scene=Scene::Town;route_.clear();destination_.reset();marching_=false;mouseX_=mouseY_=-1;fixedAnimationTime_=0;
}
}
