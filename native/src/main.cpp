#include "h3/renderer.hpp"
#include <iostream>

namespace {
std::filesystem::path bundledData(){
 char* base=SDL_GetBasePath();if(!base)return {};
 std::filesystem::path data=std::filesystem::path(base)/"GameData";SDL_free(base);
 return std::filesystem::is_regular_file(data/"assets/manifest.json")?data:std::filesystem::path{};
}
std::filesystem::path preferences(){
 char* directory=SDL_GetPrefPath("HeroesIII","HeroesIII");
 if(!directory)throw std::runtime_error("无法创建用户数据目录："+std::string(SDL_GetError()));
 std::filesystem::path path=directory;SDL_free(directory);return path;
}
}
int main(int argc,char**argv){try{
 auto bundle=bundledData();std::filesystem::path root=bundle.empty()?H3_SOURCE_ROOT:bundle,renderDir,saveDir,inputTrace,campaignSave;
 bool completionOnly=false;bool locationsOnly=false;bool mapServicesOnly=false;bool performanceOnly=false;bool localizationOnly=false;bool sitesOnly=false;bool runtimeOnly=false;bool siegeOnly=false;bool kingdomOnly=false;bool pointerOnly=false,scenarioOnly=false,interactionOnly=false,graphics=false,townArtOnly=false,displayOnly=false,fullscreen=true,runtimeInfo=false;
 for(int i=1;i<argc;i++){
  std::string arg=argv[i];
  if(arg=="--data-root"&&i+1<argc)root=argv[++i];
  else if(arg=="--save-dir"&&i+1<argc)saveDir=argv[++i];
  else if(arg=="--input-trace"&&i+1<argc)inputTrace=argv[++i];
  else if(arg=="--fullscreen")fullscreen=true;
  else if(arg=="--windowed")fullscreen=false;
  else if(arg=="--runtime-info")runtimeInfo=true;
  else if(arg=="--campaign-save"&&i+1<argc)campaignSave=argv[++i];
  else if(arg=="--performance-tests"&&i+1<argc){renderDir=argv[++i];performanceOnly=true;}
  else if(arg=="--localization-tests"&&i+1<argc){renderDir=argv[++i];localizationOnly=true;}
  else if(arg=="--completion-tests"&&i+1<argc){renderDir=argv[++i];completionOnly=true;}
  else if(arg=="--location-tests"&&i+1<argc){renderDir=argv[++i];locationsOnly=true;}
  else if(arg=="--map-service-tests"&&i+1<argc){renderDir=argv[++i];mapServicesOnly=true;}
  else if(arg=="--site-tests"&&i+1<argc){renderDir=argv[++i];sitesOnly=true;}
  else if(arg=="--runtime-tests"&&i+1<argc){renderDir=argv[++i];runtimeOnly=true;}
  else if(arg=="--siege-tests"&&i+1<argc){renderDir=argv[++i];siegeOnly=true;}
  else if(arg=="--kingdom-tests"&&i+1<argc){renderDir=argv[++i];kingdomOnly=true;}
  else if(arg=="--scenario-tests"&&i+1<argc){renderDir=argv[++i];scenarioOnly=true;}
  else if(arg=="--pointer-tests"&&i+1<argc){renderDir=argv[++i];pointerOnly=true;}
  else if(arg=="--interaction-tests"&&i+1<argc){renderDir=argv[++i];interactionOnly=true;}
  else if(arg=="--display-preview"&&i+1<argc){renderDir=argv[++i];displayOnly=true;}
  else if(arg=="--render-tests"&&i+1<argc)renderDir=argv[++i];
  else if(arg=="--town-art-preview"&&i+1<argc){renderDir=argv[++i];townArtOnly=true;}
  else if(arg=="--graphics-preview"&&i+1<argc){renderDir=argv[++i];graphics=true;}
  else if(arg=="--help"){std::cout<<"HeroesIII [--fullscreen | --windowed] [--runtime-info] [--data-root PATH] [--save-dir PATH] [--input-trace JSONL_FILE] [--pointer-tests OUTPUT_DIR] [--scenario-tests OUTPUT_DIR] [--interaction-tests OUTPUT_DIR] [--display-preview OUTPUT_DIR] [--render-tests OUTPUT_DIR] [--graphics-preview OUTPUT_DIR] [--town-art-preview OUTPUT_DIR]\nDefault startup: fullscreen. F4 toggles fullscreen/windowed mode.\n";return 0;}
  else throw std::runtime_error("未知参数："+arg);
 }
 if(root.empty()||!std::filesystem::is_regular_file(root/"assets/manifest.json"))throw std::runtime_error("找不到游戏素材。请重新安装完整应用，开发时可用 --data-root 指定素材目录。");
 if(saveDir.empty()&&!bundle.empty()&&renderDir.empty())saveDir=preferences();
 if(runtimeInfo){std::cout<<nlohmann::json{{"dataRoot",root.string()},{"bundledData",!bundle.empty()&&root==bundle},{"saveDirectory",saveDir.string()},{"sourceFallbackEnabled",std::string(H3_SOURCE_ROOT).size()>0},{"startupFullscreen",fullscreen}}.dump(2)<<std::endl;return 0;}
 // UIKit retains the frame callback after SDL_main returns.
 static std::unique_ptr<h3::Desktop> app;
 app=std::make_unique<h3::Desktop>(root,!renderDir.empty(),saveDir,inputTrace,fullscreen);
 if(!renderDir.empty()){if(completionOnly)app->completionPreview(renderDir);else if(locationsOnly)app->locationPreview(renderDir,campaignSave);else if(mapServicesOnly)app->mapServicesPreview(renderDir,campaignSave);else if(performanceOnly)app->performancePreview(renderDir,campaignSave);else if(localizationOnly)app->localizationPreview(renderDir);else if(sitesOnly)app->sitePreview(renderDir,campaignSave);else if(runtimeOnly)app->runtimePreview(renderDir);else if(siegeOnly)app->siegePreview(renderDir,campaignSave);else if(kingdomOnly)app->kingdomPreview(renderDir,campaignSave);else if(pointerOnly)app->pointerPreview(renderDir);else if(scenarioOnly)app->scenarioPreview(renderDir);else if(interactionOnly)app->interactionPreview(renderDir);else if(displayOnly)app->displayPreview(renderDir);else if(graphics||townArtOnly)app->graphicsPreview(renderDir,townArtOnly);else app->renderTests(renderDir);return 0;}
 return app->run();
}catch(const std::exception&e){std::cerr<<"Heroes III: "<<e.what()<<'\n';return 1;}}
