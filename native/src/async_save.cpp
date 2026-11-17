#include "h3/async_save.hpp"
#ifdef __APPLE__
#include <pthread/qos.h>
#endif
namespace h3 {
AsyncSaveWriter::AsyncSaveWriter(std::filesystem::path p):path_(std::move(p)){thread_=std::thread([this]{run();});}
AsyncSaveWriter::~AsyncSaveWriter(){{std::lock_guard lock(mutex_);stopping_=true;}ready_.notify_one();thread_.join();}
void AsyncSaveWriter::submit(const Game&game){auto copy=std::make_unique<Game>(game);{std::lock_guard lock(mutex_);pending_.swap(copy);}ready_.notify_one();}
void AsyncSaveWriter::flush(){std::unique_lock lock(mutex_);idle_.wait(lock,[&]{return !pending_&&!writing_;});}
std::string AsyncSaveWriter::takeError(){std::lock_guard lock(mutex_);return std::exchange(error_,{});}
uint64_t AsyncSaveWriter::writes()const{std::lock_guard lock(mutex_);return writes_;}
void AsyncSaveWriter::run(){
#ifdef __APPLE__
 pthread_set_qos_class_self_np(QOS_CLASS_UTILITY,0);
#endif
 std::unique_lock lock(mutex_);
 for(;;){ready_.wait(lock,[&]{return stopping_||pending_;});if(!pending_&&stopping_)break;
  auto snapshot=std::move(pending_);writing_=true;lock.unlock();std::string error;bool ok=false;
  try{ok=snapshot->save(path_,error);}catch(const std::exception&e){error=e.what();}
  snapshot.reset();lock.lock();writing_=false;if(ok)++writes_;else error_=std::move(error);idle_.notify_all();
 }
}
}
