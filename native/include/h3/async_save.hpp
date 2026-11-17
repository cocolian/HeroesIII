#pragma once
#include "h3/game.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
namespace h3 {
// One in-flight snapshot and at most one newer snapshot. The writer never
// touches the live game, and serializes writes to preserve their order.
class AsyncSaveWriter {
public:
 explicit AsyncSaveWriter(std::filesystem::path);
 ~AsyncSaveWriter();
 void submit(const Game&);
 void flush();
 std::string takeError();
 uint64_t writes()const;
 const std::filesystem::path& path()const{return path_;}
private:
 void run();
 std::filesystem::path path_;mutable std::mutex mutex_;std::condition_variable ready_,idle_;
 std::unique_ptr<Game> pending_;std::thread thread_;bool writing_=false,stopping_=false;uint64_t writes_=0;std::string error_;
};
}
