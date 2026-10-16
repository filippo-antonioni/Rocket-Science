#include "../../src/logger.h"
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <deque>
#include <chrono>
#include <cassert>
#include <iostream>
using Clock=std::chrono::steady_clock;
auto boot=Clock::now();
uint32_t micros(){return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-boot).count();}
uint32_t millis(){return micros()/1000;}
void delay(uint32_t n){std::this_thread::sleep_for(std::chrono::milliseconds(n));}
void vTaskDelay(unsigned n){delay(n);}
struct Queue{std::mutex mutex;std::deque<Record> data;unsigned capacity;};
std::atomic<bool> pauseWriter{false};
QueueHandle_t xQueueCreate(unsigned n,size_t size){assert(size==64);auto* q=new Queue;q->capacity=n;return q;}
int xQueueReceive(QueueHandle_t p,void* out,unsigned timeout){
 auto* q=static_cast<Queue*>(p);
 {std::lock_guard<std::mutex> lock(q->mutex);
 if(!pauseWriter && !q->data.empty()){memcpy(out,&q->data.front(),64);q->data.pop_front();return pdTRUE;}}
 delay(timeout);return 0;
}
int xQueueSend(QueueHandle_t p,const void* in,unsigned wait){
 assert(wait==0);auto* q=static_cast<Queue*>(p);std::lock_guard<std::mutex> lock(q->mutex);
 if(q->data.size()==q->capacity)return 0;
 q->data.push_back(*static_cast<const Record*>(in));return pdTRUE;
}
unsigned uxQueueMessagesWaiting(QueueHandle_t p){auto* q=static_cast<Queue*>(p);std::lock_guard<std::mutex> lock(q->mutex);return q->data.size();}
void xQueueReset(QueueHandle_t p){auto* q=static_cast<Queue*>(p);std::lock_guard<std::mutex> lock(q->mutex);q->data.clear();}
int xTaskCreatePinnedToCore(void(*fn)(void*),const char*,unsigned,void* arg,int,void*,int){std::thread(fn,arg).detach();return pdPASS;}
std::map<std::string,std::shared_ptr<std::vector<uint8_t>>> files;
FsMock LittleFS;
bool FsMock::begin(bool){return true;}
size_t FsMock::totalBytes(){return 4*1024*1024;}
size_t FsMock::usedBytes(){return 0;}
bool FsMock::exists(const char* p){return files.count(p);}
File FsMock::open(const char* name,const char*){File f;f.bytes=std::make_shared<std::vector<uint8_t>>();files[name]=f.bytes;return f;}
File::operator bool() const{return bool(bytes);}
size_t File::write(const uint8_t* b,size_t n){bytes->insert(bytes->end(),b,b+n);return n;}
void File::flush(){}
void File::close(){bytes.reset();}
void waitClose(Logger* log){for(int i=0;i<2000 && log->active();++i)delay(1);assert(!log->active());}
Record rec(unsigned n){Record r={};r.sequenza=n;r.tMs=n*10;return r;}
int main(){
 auto* log=new Logger;assert(log->init());assert(log->open(1));
 for(unsigned i=0;i<1000;++i)log->feed(rec(i),false,false);
 delay(30);assert(log->rows()==0); // PAD: nessun record trasferito alla flash
 log->feed(rec(1000),true,false);
 for(unsigned i=1001;i<=1050;++i)log->feed(rec(i),false,i==1050);
 waitClose(log);assert(log->ok() && log->dropped()==0 && log->rows()==351);
 const auto& bytes=*files[log->name()];assert(bytes.size()==32+351*64);
 for(unsigned i=0;i<351;++i){Record r;memcpy(&r,bytes.data()+32+i*64,64);assert(r.sequenza==700+i);assert(r.crc==crc32(&r,60));}
 assert(log->open(7));for(unsigned i=0;i<1000;++i)log->feed(rec(i),false,false);
 log->finish(true);waitClose(log);assert(log->rows()==300);
 pauseWriter=true;assert(log->open(2));
 for(unsigned i=0;i<600;++i)log->feed(rec(i),i==0,false);
 assert(!log->ok() && log->dropped()>0);log->finish();pauseWriter=false;waitClose(log);
 std::cout<<"PASS logger: PAD senza record flash, prebuffer ordinato, nessun duplicato, chiusura LANDED, snapshot PAD, overflow rilevato\n"<<std::flush;
 std::_Exit(0); // il task FreeRTOS simulato e' intenzionalmente permanente
}
