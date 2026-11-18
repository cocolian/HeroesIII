#pragma once
#include "h3/async_save.hpp"
#include "h3/game.hpp"
#include "h3/masks.hpp"
#include "h3/audio.hpp"
#include "h3/battle_motion.hpp"
#include "h3/native_pointer.hpp"
#include "h3/display_layout.hpp"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <functional>
#include <map>
#include <unordered_map>

namespace h3 {
enum class Scene {Menu,Adventure,Town,Build,Recruit,Army,Split,Hero,Spells,Battle,Level,Report,Help,Codex,Scenario,University,MageGuild,Market,BuildConfirm,Fort,Options,Outcome,Visit,Puzzle,Maps,Tavern,Journal,Meeting,Neutral,TownService,Overview,BattleExit,Necromancy,PortalChoice,WorldSurvey};
struct Button {
 SDL_Rect rect;std::function<void()> action;bool enabled=true;std::function<bool(Point)> hit;std::string hint;
 Button(SDL_Rect bounds,std::function<void()> callback,bool active=true,std::function<bool(Point)> shape={},std::string description={}):rect(bounds),action(std::move(callback)),enabled(active),hit(std::move(shape)),hint(std::move(description)){}
 bool contains(Point p)const{return p.x>=rect.x&&p.y>=rect.y&&p.x<rect.x+rect.w&&p.y<rect.y+rect.h&&(!hit||hit(p));}
};
struct Image {SDL_Texture* texture=nullptr;int w=0,h=0;};
class Desktop {
public:
 explicit Desktop(std::filesystem::path root,bool hidden=false,std::filesystem::path saveDirectory={},std::filesystem::path inputTrace={},bool startFullscreen=false);~Desktop();int run();void render();bool screenshot(const std::filesystem::path&);void renderTests(const std::filesystem::path&);void graphicsPreview(const std::filesystem::path&,bool townArtOnly=false);void displayPreview(const std::filesystem::path&);void interactionPreview(const std::filesystem::path&);void runtimePreview(const std::filesystem::path&);void siegePreview(const std::filesystem::path&,const std::filesystem::path&);void kingdomPreview(const std::filesystem::path&,const std::filesystem::path&);void scenarioPreview(const std::filesystem::path&);void pointerPreview(const std::filesystem::path&);
 void sitePreview(const std::filesystem::path&,const std::filesystem::path&);
 void mapServicesPreview(const std::filesystem::path&,const std::filesystem::path&);void thievesGuildDialog();
 bool locationDialog();void completionPreview(const std::filesystem::path&);void locationPreview(const std::filesystem::path&,const std::filesystem::path&);
 void localizationPreview(const std::filesystem::path&);
 void performancePreview(const std::filesystem::path&,const std::filesystem::path&);
 std::unique_ptr<AsyncSaveWriter> autosave_;
 bool asynchronousSave_=false,autosaveDirty_=false;uint64_t nextAutosave_=0;
 void flushAutosave();
 struct MapSpriteInfo {int width=0,height=0;Point visitOffset{};};
 std::unordered_map<std::string,MapSpriteInfo> mapSpriteInfo_;
 SDL_Rect mapSpriteCanvasAt(const std::string&,Point,bool,SDL_Rect,const std::array<float,2>&)const;
 Game game=Game::create();Scene scene=Scene::Menu;
private:
 std::filesystem::path root_,saveDir_,inputTrace_;SDL_Window* window_=nullptr;SDL_Renderer* renderer_=nullptr;std::map<int,TTF_Font*> fonts_;std::string fontPath_,titleFontPath_;double pixelScale_=1;
 std::unordered_map<std::string,Image> images_,texts_;Image canvas_;nlohmann::json manifest_,demo_,hd_,mapArt_,ownerArt_;std::vector<Button> buttons_;std::vector<Step> route_;std::optional<Point> destination_;float camX_=4.5f,camY_=20.5f,zoom_=1;
 Point heroDirection_{1,0};
 bool adventureCursorHidden_=false,cursorPreview_=false;
 struct FogFrame {int index=0;bool flipX=false,flipY=false;};
 std::array<FogFrame,256> fogFrames_{};bool fogFramesReady_=false;
 struct ArtTexture {SDL_Texture*texture=nullptr;size_t bytes=0;uint64_t used=0;};struct ArtSource {SDL_Surface*surface=nullptr;size_t bytes=0;uint64_t used=0;};
 std::unordered_map<std::string,ArtTexture> enhanced_;std::unordered_map<std::string,ArtSource> artSources_;size_t enhancedBytes_=0,sourceBytes_=0;uint64_t artUse_=0;bool redrawnBattleDrawn_=false,redrawnTownDrawn_=false;
 struct ArtMesh {std::string file;std::vector<SDL_Vertex> vertices;std::vector<int> indices;};std::unordered_map<std::string,std::vector<ArtMesh>> redrawnMeshes_;
 uint64_t animationTime_=0;std::optional<uint64_t> fixedAnimationTime_;bool autoCombat_=false;BattleMotion battleMotion_;
 std::unique_ptr<AudioEngine> audio_;ClientSettings settings_;Scene optionsReturn_=Scene::Menu;int optionsDragging_=-1;uint64_t scrollTick_=0;std::optional<Point> nativePointer_;
 enum class MenuView {Main,Scores,Credits};MenuView menuView_=MenuView::Main;
 nlohmann::json mapLibrary_;bool scenarioVideoPreset_=false;int selectedMap_=0,mapPage_=0,mapLevel_=0,scenarioHero_=-1,scenarioDifficulty_=1;
 int visitPage_=0;std::string visitMessage_;
 int locationObject_=0,locationPage_=0,locationSelected_=-1,locationAmount_=1;
 void mapLibrary();void selectMap(int);void startScenario();void scenarioRenderTests(const std::filesystem::path&);
 std::unique_ptr<NativePointerInput> pointerInput_;
 std::optional<SDL_Keymod> pointerModifiers_;
 std::map<SDL_FingerID,Point> touchPoints_;std::optional<SDL_FingerID> touchPrimary_;
 Point touchOrigin_{};Uint32 touchStarted_=0;bool touchPinching_=false,background_=false;double touchDistance_=0;
 Scene marketReturn_=Scene::Adventure;int marketObject_=0,marketSold_=-1,marketBought_=-1,marketLots_=0;bool marketDragging_=false,giftMode_=false;int giftOwner_=-1;int marketLimit()const;std::string marketCommit();
 bool minimapDragging_=false;bool mouseLeftDown_=false;int pressedButton_=-1;Scene pressedScene_=Scene::Menu;
 std::optional<SDL_Rect> hintBounds_;bool rightHelp_=false;Point hintAnchor_{};uint64_t hintSince_=0;Scene hintScene_=Scene::Menu;std::string hintText_;
 std::unordered_map<std::string,PixelMask> masks_;Scene buildingReturn_=Scene::Build,recruitReturn_=Scene::Town,splitReturn_=Scene::Army,armyReturn_=Scene::Adventure;int selectedBuilding_=TownHall;bool recruitDragging_=false;
 struct ArmyDrag {bool garrison=false;int slot=-1;Point start{};bool moving=false;};std::optional<ArmyDrag> armyDrag_;
 bool running_=true,marching_=false,drag_=false,dragged_=false,fullscreen_=false,spellAdventure_=false;int neutralOfferId_=0,neutralReplaceSlot_=-1;int pendingAdventure_=-1,worldSurveySpell_=ViewEarth;int spellFilter_=0,spellPage_=0,scenarioFaction_=0,codexFaction_=0,codexUnit_=0,focus_=-1,mouseX_=0,mouseY_=0,dragX_=0,dragY_=0,buildPage_=0,recruitUnit_=Pikeman,recruitAmount_=1,pendingSpell_=-1,sacrificeTarget_=-1,sourceSlot_=-1,targetSlot_=-1,splitAmount_=1;bool recruitGarrison_=false,sourceGarrison_=false,targetGarrison_=false;float dragCamX_=0,dragCamY_=0,dragZoom_=1;uint64_t nextStep_=0,nextAI_=0,toastUntil_=0;std::string toast_;
 Image& image(const std::string&,bool keyed=true,bool blue=false);void art(const std::string&,SDL_Rect dest,std::optional<SDL_Rect> src={},bool keyed=true,bool flip=false,bool blue=false);void text(const std::string&,int x,int y,int size=13,SDL_Color color={244,224,174,255},int wrap=0);void box(SDL_Rect,SDL_Color);void outline(SDL_Rect,SDL_Color);void panel(SDL_Rect);void button(SDL_Rect,const std::string&,std::function<void()>,bool enabled=true,bool invisible=false,const std::string& hint={});void portrait(int,SDL_Rect,bool blue=false);void heroPortrait(const Hero&,SDL_Rect);bool siegeArtwork(const Battle&);void title(const std::string&,std::optional<Scene> back=Scene::Adventure);void resourceBar(int y=578,int width=600);void say(const std::string&);void persist();void load(bool automatic=false);void settle();void click(Point,bool shift);void key(SDL_Keycode,SDL_Keymod);void tick();Point mapPoint(Point)const;Point mapScreen(Point)const;
 std::string buttonHintAt(Point)const;void drawButtonHint();std::string originalButtonHint(const std::string&,int)const;void hintRenderTests(const std::filesystem::path&);void encounterRenderTests(const std::filesystem::path&);
 SDL_Surface* readSurface(const std::string&,bool,bool);SDL_Texture*cacheArtwork(const std::string&,SDL_Surface*);bool enhancedArt(const std::string&,SDL_Rect,SDL_Rect,bool,bool,bool);bool redrawnBackground(const std::string&,SDL_Rect);bool redrawnBattle();bool redrawnTown();bool redrawnTownBuilding(const TownLayer&);int redrawnBuildingsDrawn_=0;void clearArtworkCache();int fontKey(int)const;TTF_Font*font(int);void graphicsRenderTests(const std::filesystem::path&);
 SDL_Texture* buildingArtworkTexture(const nlohmann::json&,const nlohmann::json&);
 SDL_Texture* originalTownClothTexture(const std::string&,int,const std::vector<SDL_Rect>&);
 bool redrawnAnimatedTownBuilding(const TownLayer&);int redrawnAnimatedBuildingsDrawn_=0;
 void townArtworkRenderTests(const std::filesystem::path&);
 bool originalImage(const std::string&,SDL_Rect,std::optional<SDL_Rect> src={});int frameCount(const std::string&,int group=0)const;bool frame(const std::string&,int group,int index,SDL_Rect canvas,bool flip=false,bool blue=false,bool vertical=false,int owner=-2);bool townAnimation(const TownLayer&);bool combatImage(const CombatStack&,int group,bool last=false,bool selected=false,bool targeted=false);void battleCount(const CombatStack&,Point);
 struct CombatVisual {std::string animation;int group=2,index=0;SDL_Rect canvas{};bool flip=false;};
 std::optional<CombatVisual> combatVisual(const CombatStack&,int group=2,bool last=false)const;
 bool combatBodyContains(const CombatStack&,Point);const CombatStack* battleTargetAt(Point);void battleCellAction(Point);void battleTargetRenderTests(const std::filesystem::path&);
 void openMarket(int objectId=0);void market();void marketAmount(int mouseX);SDL_Rect marketTrack()const;void originalButton(SDL_Rect,const std::string&,int normal,std::function<void()>,bool enabled=true,int disabled=-1,const std::string& hint={});void marketIcon(int resource,SDL_Rect);
 void handleEvent(const SDL_Event&);
 void handleNativeEvent(SDL_Event,bool precisePointer=false,SDL_Keymod=KMOD_NONE);Point windowPoint(Point)const;void movePointer(Point);
 void touchEvent(const SDL_TouchFingerEvent&);void cancelPointer(bool clearTouch=true);void frameStep();void touchRenderTests(const std::filesystem::path&);
 const PixelMask& pixelMask(const std::string&);bool layerVisible(const TownLayer&)const;int townLayerAt(Point);std::optional<std::pair<bool,int>> townArmyAt(Point)const;void townArmyRow(bool);void townBuilding(int);void buildingPopup(int,Scene);void buildConfirm();void fort();void openRecruit(int,Scene);void unitPreview(int,SDL_Rect,bool fort=false);void recruitAmount(int);SDL_Rect recruitTrack()const;void townFooter(bool hall=false);void townDrag(Point);
 void centerText(const std::string&,SDL_Rect,int size=13,SDL_Color color={244,224,174,255});int recruitLimit()const;void purchaseRecruit();
 void rightText(const std::string&,SDL_Rect,int size=13,SDL_Color color={244,224,174,255});
 void townRenderTests(const std::filesystem::path&);
 void updateAudio();std::string audioTheme()const;void openOptions();void options();void applyOptions();void optionVolume(int,int);SDL_Rect volumeTrack(int)const;
 void optionsRenderTests(const std::filesystem::path&);
 void battleAction(const std::function<std::string()>&);void battleRenderTests(const std::filesystem::path&);void battleMotionRenderTests(const std::filesystem::path&);void battleActionRenderTests(const std::filesystem::path&);
 void moraleRenderTests(const std::filesystem::path&);
 void battleHealthRenderTests(const std::filesystem::path&);
 void pointerRenderTests(const std::filesystem::path&);void mapGestureRenderTests(const std::filesystem::path&);
 void runtimeFlowRenderTests(const std::filesystem::path&);
 void mineIncomeRenderTests(const std::filesystem::path&);
 void displayRenderTests(const std::filesystem::path&);void adventureArtworkRenderTests(const std::filesystem::path&);void campaignFlowRenderTests(const std::filesystem::path&);
 SDL_Rect mapSpriteCanvas(const std::string&,Point,bool visitable=true)const;void mapSprite(const std::string&,Point,bool visitable=true,int owner=-2);void adventureObjects();void adventureTerrain();void mapArtworkRenderTests(const std::filesystem::path&);
 struct TerrainVisual {std::string asset;int frame=0;bool flipX=false,flipY=false;};TerrainVisual terrainVisual(Point)const;void terrainRenderTests(const std::filesystem::path&);
 std::string ownerName(int)const;SDL_Color ownerColor(int)const;std::string ownerHeroFlag(int)const;std::string ownerPage(const std::string&,int page,int owner)const;void ownershipRenderTests(const std::filesystem::path&);
 bool menuButtonContains(const std::string&,SDL_Rect,Point);void menuButton(SDL_Rect,const std::string&,std::function<void()>);void menuRenderTests(const std::filesystem::path&);
 std::string mapObjectAsset(const Object&)const;bool mapSpriteContains(const std::string&,Point,Point,bool visitable=true);const Object* adventureObjectAt(Point);
 int adventureCursorFrame();void drawAdventureCursor();void restoreSystemCursor();void cursorRenderTests(const std::filesystem::path&);
 struct CursorVisual {std::string asset;int frame=0;Point hotspot{};};
 std::optional<CursorVisual> combatCursor();void combatCursorRenderTests(const std::filesystem::path&);
 void mapDialog(SDL_Rect,Scene background=Scene::Adventure);void visitDialog();void puzzle();void adventureVisitRenderTests(const std::filesystem::path&);std::optional<AdventureVisit> visitPopup_;uint64_t puzzleArtKey_=0;
 void outcome();void afterReport();void outcomeRenderTests(const std::filesystem::path&);
 std::array<float,2> mapCamera()const;Point canvasLogicalSize()const;SDL_Rect adventureMapRect()const;SDL_Rect adventureMinimapRect()const;void adventureFrame();void adventureRoute();void adventureHero();void prepareFogFrames();void adventureFog();void toggleFullscreen();void resetRenderTextures(bool all);
 int garrisonObject_=0;Army& garrisonArmy();std::string transferArmy(bool,int,bool,int,int);int heroArtifact_=-1,heroArtifactPage_=0;int meetingHero_=0,journalPage_=0,heroListPage_=0,townListPage_=0;void activateHero(int);void tavern();void journal();void meeting();void neutralDialog();int townServiceBuilding_=0,overviewPage_=0;bool overviewTowns_=false,battleExitSurrender_=false;void townServices();void kingdomOverview();void battleExitDialog();int necromancySlot_=-1;void necromancyDialog();
 void adventureSpellPanel();void worldSurvey();void openSpellbook();void mageGuild();void university();void scenario();void codex();void menu();void adventure();void townScene();void town();void build();void recruit();void army();void split();void hero();void spellbook();void battle();void level();void report();void help();void armyRow(const Army&,int y,bool garrison,int x=70,int slotW=76);void selectStack(bool,int,bool);std::string cost(const Resources&)const;
};
}
