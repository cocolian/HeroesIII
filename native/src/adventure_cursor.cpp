#include "h3/renderer.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <tuple>

namespace h3 {
bool Desktop::mapSpriteContains(const std::string&name,Point position,Point point,bool visitable){
 auto canvas=mapSpriteCanvas(name,position,visitable);SDL_Point at{point.x,point.y};
 if(canvas.w<=0||canvas.h<=0||!SDL_PointInRect(&at,&canvas))return false;
 auto key="#map-hit#"+name;auto found=masks_.find(key);
 if(found==masks_.end()){
  const auto&animation=demo_["def"][name];const auto&frames=animation["groups"]["0"];
  PixelMask mask;mask.width=frames[0]["canvas"][0];mask.height=frames[0]["canvas"][1];mask.pixels.resize(size_t(mask.width)*mask.height);
  std::vector<SDL_Surface*> pages;for(size_t i=0;i<animation["pages"].size();i++){auto colored=ownerPage(name,int(i),-1);pages.push_back(readSurface(colored.empty()?animation["pages"][i]["file"].get<std::string>():colored,false,false));}
  // Union animated silhouettes so rotating blades do not flicker between
  // interaction and ground. Transparent padding and shadows never count.
  for(const auto&f:frames){auto*page=pages[f["page"].get<int>()];if(!page)continue;
   int left=f["offset"][0],top=f["offset"][1],sx=f["rect"][0],sy=f["rect"][1],w=f["rect"][2],h=f["rect"][3];
   for(int y=0;y<h&&y+top<mask.height;++y)for(int x=0;x<w&&x+left<mask.width;++x){
    auto*pixel=static_cast<Uint8*>(page->pixels)+(sy+y)*page->pitch+(sx+x)*4;
    if(pixel[3]>128)mask.pixels[size_t(y+top)*mask.width+x+left]=1;
   }
  }
  for(auto*page:pages)if(page)SDL_FreeSurface(page);found=masks_.emplace(key,std::move(mask)).first;
 }
 const auto&mask=found->second;
 return mask.contains((point.x-canvas.x)*mask.width/canvas.w,(point.y-canvas.y)*mask.height/canvas.h);
}
const Object* Desktop::adventureObjectAt(Point point){
 SDL_Point cursor{point.x,point.y};auto clip=adventureMapRect();if(!SDL_PointInRect(&cursor,&clip))return nullptr;
 auto tile=mapPoint(point);auto*ground=game.tile(tile);if(!ground||!ground->seen)return nullptr;
 // Preserve direct visit-tile selection, including the existing path controls.
 if(auto*object=game.objectAt(tile))return object;
 const Object*selected=nullptr;std::string selectedAsset;
 for(const auto&object:game.objects){
  if(object.level!=game.mapLevel||object.type==ObjectType::MapEvent||!game.tile(object.pos)->seen)continue;auto asset=mapObjectAsset(object);
  if(object.type==ObjectType::Town||object.type==ObjectType::Monster)continue;
  if(tile!=object.pos&&!mapSpriteContains(asset,object.sourceClass>=0?object.anchor:object.pos,point,object.sourceClass<0))continue;
  if(!selected||std::tuple(object.pos.y,object.pos.x,asset)>std::tuple(selected->pos.y,selected->pos.x,selectedAsset)){selected=&object;selectedAsset=std::move(asset);}
 }
 if(!selected)return nullptr;
 for(int y=selected->pos.y;y<game.mapHeight;++y)for(int x=0;x<game.mapWidth;++x){
  Point position{x,y};const auto&t=*game.tile(position);
  if(!t.seen||t.decor==Decor::None||std::pair(y,x)<=std::pair(selected->pos.y,selected->pos.x))continue;
  const char*asset=t.decor==Decor::Tree?"avlpntr0.def":"avlmtgr5.def";
  if(mapSpriteContains(asset,position,point,false))return nullptr;
 }
 return selected;
}
int Desktop::adventureCursorFrame(){
 if(scene!=Scene::Adventure||background_||game.result||game.battle||game.report||!game.hero.levelChoices.empty()||(drag_&&dragged_)||minimapDragging_||touchPrimary_||touchPinching_)return -1;
 SDL_Point point{mouseX_,mouseY_};auto clip=adventureMapRect();if(!SDL_PointInRect(&point,&clip))return -1;
 auto*object=adventureObjectAt({mouseX_,mouseY_});auto goal=object?object->visitPosition(mapPoint({mouseX_,mouseY_})):mapPoint({mouseX_,mouseY_});
 auto*tile=game.tile(goal);if(!tile||!tile->seen)return 0;
 if(object&&object->type==ObjectType::Town&&object->owner==0)return 3;
 if(goal==game.hero.pos)return object?9:2;
 auto path=game.path(goal);if(path.empty())return 0;
 int turn=0,remaining=game.hero.movement,allowance=game.movementAllowance();
 for(const auto&step:path){if(step.cost>remaining){++turn;remaining=allowance;}remaining-=step.cost;}
 int action=(object&&(object->type==ObjectType::Monster||object->type==ObjectType::EnemyHero))||game.guardingMonster(goal,true)||(object&&object->type==ObjectType::Town&&object->owner!=0)?5:object?9:4;
 // Consecutive cursor images encode travel days; they are not animation frames.
 return action+6*std::min(turn,3);
}
void Desktop::restoreSystemCursor(){if(adventureCursorHidden_){SDL_ShowCursor(SDL_ENABLE);adventureCursorHidden_=false;}}
void Desktop::drawAdventureCursor(){
 std::optional<CursorVisual> visual;
 if(scene==Scene::Battle)visual=combatCursor();
 else if(int index=adventureCursorFrame();index>=0){
  Point pivot{};
  if(index==2)pivot={12,10};else if(index==3)pivot={12,12};
  else if(index>=4&&index<=27){int action=(index-4)%6;pivot=action==0?Point{15,13}:action==1?Point{13,13}:Point{14,16};}
  visual=CursorVisual{"cradvntr.def",index,pivot};
 }
 auto flags=SDL_GetWindowFlags(window_);
 bool visible=visual&&(cursorPreview_||((flags&SDL_WINDOW_INPUT_FOCUS)&&(flags&SDL_WINDOW_MOUSE_FOCUS)));
 if(!visible||!frameCount(visual->asset)){restoreSystemCursor();return;}
 const auto&animation=demo_["def"][visual->asset];const auto&f=animation["groups"]["0"][visual->frame];
 auto file=animation["pages"][f["page"].get<int>()]["file"].get<std::string>();auto&texture=image(file,false);
 if(!texture.texture){restoreSystemCursor();return;}
 if(!adventureCursorHidden_){SDL_ShowCursor(SDL_DISABLE);adventureCursorHidden_=true;}
 SDL_Rect source{f["rect"][0],f["rect"][1],f["rect"][2],f["rect"][3]};
 SDL_Rect destination{mouseX_-visual->hotspot.x+f["offset"][0].get<int>(),mouseY_-visual->hotspot.y+f["offset"][1].get<int>(),source.w,source.h};
 SDL_SetTextureScaleMode(texture.texture,SDL_ScaleModeNearest);SDL_RenderCopy(renderer_,texture.texture,&source,&destination);
}
void Desktop::cursorRenderTests(const std::filesystem::path&directory){
 auto check=[](bool ok,const char*message){if(!ok)throw std::runtime_error(message);};
 int w=0,h=0;SDL_GetWindowSize(window_,&w,&h);SDL_SetWindowSize(window_,1000,750);
 auto setup=[&]{game=Game::create();for(auto&t:game.tiles){t.seen=true;t.decor=Decor::None;}scene=Scene::Adventure;settings_=ClientSettings{};settings_.hdArtwork=false;camX_=4.5f;camY_=20.5f;zoom_=1;marching_=drag_=dragged_=false;route_.clear();destination_.reset();mouseX_=mouseY_=-1;fixedAnimationTime_=0;nextStep_=nextAI_=scrollTick_=0;toastUntil_=0;};setup();
 auto pointFor=[&](int id,bool body){
  const auto*object=game.objectById(id);auto asset=mapObjectAsset(*object);auto canvas=mapSpriteCanvas(asset,object->pos);auto clip=adventureMapRect();
  for(int y=std::max(canvas.y,clip.y);y<std::min(canvas.y+canvas.h,clip.y+clip.h);++y)for(int x=std::max(canvas.x,clip.x);x<std::min(canvas.x+canvas.w,clip.x+clip.w);++x){
   Point point{x,y};if(body&&mapPoint(point)==object->pos)continue;
   if(mapSpriteContains(asset,object->pos,point)){auto*hit=adventureObjectAt(point);if(hit&&hit->id==id)return point;}
  }
  throw std::runtime_error("Cursor fixture has no visible object body: "+std::to_string(id));
 };
 auto shot=[&](const char*name){toastUntil_=0;render();check(screenshot(directory/(std::string(name)+".png")),"Cursor screenshot failed");};
 auto before=game.toJson();nlohmann::json items=nlohmann::json::array();
 for(const auto&object:game.objects){
  if(object.type==ObjectType::Town||object.type==ObjectType::Monster)continue;
  camX_=object.pos.x+.5f;camY_=object.pos.y+.5f;auto point=pointFor(object.id,false);movePointer(point);
  check(adventureCursorFrame()>=9&&(adventureCursorFrame()-9)%6==0,"Visitable map item did not use the original horse cursor");items.push_back({{"id",object.id},{"type",int(object.type)},{"name",object.name}});
 }
 check(game.toJson()==before,"Hovering a map item changed game state or RNG");
 auto wind=std::find_if(game.objects.begin(),game.objects.end(),[](const auto&o){return o.type==ObjectType::Windmill;});int windId=wind->id;Point windPosition=wind->pos;
 cursorPreview_=true;
 for(int percent:{65,100,180}){zoom_=percent/100.f;camX_=windPosition.x+.5f;camY_=windPosition.y+.5f;movePointer(pointFor(windId,true));check(adventureCursorFrame()==9,"Scaled windmill body lost its interaction cursor");shot(("cursor-windmill-"+std::to_string(percent)).c_str());check(adventureCursorHidden_,"Software cursor did not suppress the system arrow");}
 zoom_=1;auto point=pointFor(windId,true);movePointer(point);click(point,false);
 check(destination_==windPosition&&!route_.empty(),"Clicking a windmill blade routed to the painted tile instead of its visit tile");shot("cursor-windmill-route");
 // A fixed pointer on rotating blades must keep the same interaction target.
 for(int frame=0;frame<8;++frame){fixedAnimationTime_=frame*180;check(adventureObjectAt(point)&&adventureObjectAt(point)->id==windId&&adventureCursorFrame()==9,"Windmill animation flickered the hover target");}
 drag_=dragged_=true;check(adventureCursorFrame()<0,"Dragging the map retained the visit cursor");drag_=false;check(adventureCursorFrame()==9,"A finished map drag permanently suppressed the visit cursor");dragged_=false;
 check(game.toJson()==before,"Cursor selection or path preview mutated gameplay");
 game.tile(windPosition)->seen=false;check(adventureCursorFrame()!=9,"Hidden map object leaked through the cursor");game.tile(windPosition)->seen=true;
 game.tile(windPosition)->decor=Decor::Tree;check(adventureCursorFrame()==0,"Unreachable object advertised a visit action");game.tile(windPosition)->decor=Decor::None;
 scene=Scene::Town;render();check(!adventureCursorHidden_,"Town retained the adventure horse cursor");scene=Scene::Adventure;
 movePointer({-1,-1});render();check(adventureCursorFrame()<0&&!adventureCursorHidden_,"Leaving the map retained a hidden system cursor");
 setup();cursorPreview_=true;auto*chest=game.objectById(7);Point chestPosition=chest->pos;point=pointFor(7,false);movePointer(point);shot("cursor-chest-hover");
 check(adventureCursorFrame()==9,"Treasure hover did not display the visit horse");click(point,false);key(SDLK_m,KMOD_NONE);
 for(int step=1;step<=20&&marching_;++step){fixedAnimationTime_=step*400;tick();}
 check(game.hero.pos==chestPosition&&!game.objectById(7)&&game.resources[Gold]==11500,"Horse-cursor click did not preserve treasure collection");
 render();check(adventureCursorFrame()!=9,"Collected treasure left a stale horse cursor");shot("cursor-chest-collected");
 setup();cursorPreview_=true;game.hero.pos=windPosition;point=pointFor(windId,true);movePointer(point);auto resources=game.resources;click(point,false);
 check(game.objectById(windId)->visited&&game.resources!=resources&&scene==Scene::Visit,"Clicking the current windmill did not show its visit result");render();key(SDLK_RETURN,KMOD_NONE);resources=game.resources;click(point,false);check(scene==Scene::Visit&&game.resources==resources,"Repeated windmill click paid twice");shot("cursor-windmill-revisited");render();key(SDLK_RETURN,KMOD_NONE);
 SDL_Event leave{};leave.type=SDL_WINDOWEVENT;leave.window.event=SDL_WINDOWEVENT_FOCUS_LOST;handleEvent(leave);check(!adventureCursorHidden_,"Losing window focus left the system cursor hidden");
 setup();cursorPreview_=true;game.objects.clear();game.hero.pos={2,2};game.hero.movement=100;
 for(auto&t:game.tiles){t.seen=true;t.decor=Decor::None;t.terrain=Terrain::Grass;t.road=false;}
 for(auto [x,expected]:{std::pair{3,4},std::pair{4,10},std::pair{19,16},std::pair{35,22}}){
  camX_=x+.5f;camY_=2.5f;movePointer(mapScreen({x,2}));check(adventureCursorFrame()==expected,"Travel-day cursor disagreed with per-step daily movement");shot(("cursor-ground-turn-"+std::to_string((expected-4)/6+1)).c_str());
 }
 camX_=2.5f;camY_=2.5f;movePointer(mapScreen(game.hero.pos));check(adventureCursorFrame()==2,"Current hero did not use the original hero cursor");shot("cursor-hero");
 game.tile({3,2})->decor=Decor::Mountain;movePointer(mapScreen({3,2}));check(adventureCursorFrame()==0,"Blocked ground did not use the original gold pointer");shot("cursor-blocked-ground");
 setup();movePointer(mapScreen(game.hero.pos));check(adventureCursorFrame()==3,"Owned town did not use the original castle cursor");shot("cursor-owned-town");
 cursorPreview_=false;movePointer({-1,-1});scene=Scene::Town;shot("cursor-town-default");
 std::ofstream(directory/"cursor-audit.json")<<nlohmann::json{{"asset","cradvntr.def"},{"frame",9},{"originalName","CursrA24.pcx"},{"logicalCanvas",{40,40}},{"hotspot",{14,16}},{"groundWalk",4},{"travelDayFrames",{4,10,16,22}},{"dayVariantsAreNotAnimation",true},{"ownedTown",3},{"selectedHero",2},{"blockedGround",0},{"visitedItemKinds",items},{"zooms",{65,100,180}},{"spriteBodyClickTargetsVisitTile",true},{"stableAcrossAnimationFrames",true},{"hoverDoesNotMutateGameOrRng",true},{"hiddenAndUnreachableObjectsExcluded",true},{"cursorRestoredOnSceneExitAndFocusLoss",true},{"treasureCollectedNormally",true},{"windmillCanBeRevisitedByClick",true}}.dump(2)<<'\n';
 restoreSystemCursor();game=Game::create();scene=Scene::Town;settings_=ClientSettings{};zoom_=1;route_.clear();destination_.reset();marching_=false;mouseX_=mouseY_=-1;fixedAnimationTime_=0;SDL_SetWindowSize(window_,w,h);
}
}
