#include "h3/async_save.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace h3;
int main(){int checks=0;try{
 auto check=[&](bool ok,const char*m){if(!ok)throw std::runtime_error(m);++checks;};
 auto dir=std::filesystem::temp_directory_path()/("heroesiii-writer-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);auto path=dir/"autosave.json";Game game=Game::create();std::string error;
 {
  AsyncSaveWriter writer(path);for(int i=0;i<40;++i){game.day=i+1;game.resources[Gold]=1000+i;writer.submit(game);}
  writer.flush();Game loaded;check(loaded.load(path,error)&&loaded.toJson()==game.toJson(),"Latest queued snapshot was lost or reordered");check(writer.writes()>=1&&writer.writes()<=40,"Writer did not complete any snapshot");check(writer.takeError().empty(),"Valid snapshots reported an error");
  auto before=game.toJson();writer.submit(game);game.resources[Gold]=999999;writer.flush();check(loaded.load(path,error)&&loaded.toJson()==before,"Writer held a reference to the live mutable game");
 }
 {AsyncSaveWriter writer(path);writer.submit(game);}Game loaded;check(loaded.load(path,error)&&loaded.toJson()==game.toJson(),"Shutdown discarded the pending snapshot");
 auto invalid=dir/"not-a-directory";{std::ofstream f(invalid);f<<"x";} {AsyncSaveWriter writer(invalid/"autosave.json");writer.submit(game);writer.flush();check(!writer.takeError().empty(),"Background I/O failure was lost");check(writer.takeError().empty(),"One I/O error was reported repeatedly");}
 std::filesystem::remove_all(dir);std::cout<<checks<<" background save checks passed\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
