#include "../../firmware/banco/logger.h"
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
QueueHandle_t xQueueCreate(unsigned n,size_t size){assert(size==sizeof(Record));auto* q=new Queue;q->capacity=n;return q;}
int xQueueReceive(QueueHandle_t p,void* out,unsigned timeout){
 auto* q=static_cast<Queue*>(p);
 {std::lock_guard<std::mutex> lock(q->mutex);
 if(!pauseWriter && !q->data.empty()){memcpy(out,&q->data.front(),sizeof(Record));q->data.pop_front();return pdTRUE;}}
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
std::atomic<unsigned> writeCalls{0},flushCalls{0};
std::atomic<bool> failWrite{false};
bool psramAvailable=true;
void* allocatedRam=nullptr;
void* heap_caps_malloc(size_t n,uint32_t caps){
 assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
 if(!psramAvailable)return nullptr;
 allocatedRam=std::malloc(n);return allocatedRam;
}
FsMock LittleFS;
bool FsMock::begin(bool){return true;}
size_t FsMock::totalBytes(){return 4*1024*1024;}
size_t FsMock::usedBytes(){return 0;}
bool FsMock::exists(const char* p){return files.count(p);}
File FsMock::open(const char* name,const char*){File f;f.bytes=std::make_shared<std::vector<uint8_t>>();files[name]=f.bytes;return f;}
File::operator bool() const{return bool(bytes);}
size_t File::write(const uint8_t* b,size_t n){++writeCalls;if(failWrite)return 0;bytes->insert(bytes->end(),b,b+n);return n;}
size_t File::size(){return bytes?bytes->size():0;}
void File::flush(){++flushCalls;}
void File::close(){bytes.reset();}
void waitClose(Logger* log){for(int i=0;i<20000 && log->active();++i)delay(1);assert(!log->active());}
Record rec(unsigned n){Record r={};r.sequenza=n;r.tMs=n*10;return r;}
int main(){
 auto* log=new Logger;assert(log->init());assert(log->open(0));
 for(unsigned i=0;i<1000;++i)log->feed(rec(i),false,false);
 delay(30);assert(log->rows()==0); // PAD: nessun record trasferito alla flash
 log->feed(rec(1000),true,false);
 for(unsigned i=1001;i<=1050;++i)log->feed(rec(i),false,i==1050);
 waitClose(log);assert(log->ok() && log->dropped()==0 && log->rows()==351);
 const auto& bytes=*files[log->name()];assert(bytes.size()==sizeof(LogHeader)+351*sizeof(Record));
 for(unsigned i=0;i<351;++i){Record r;memcpy(&r,bytes.data()+sizeof(LogHeader)+i*sizeof(Record),sizeof(Record));assert(r.sequenza==700+i);assert(r.crc==crc32(&r,offsetof(Record,crc)));}
 assert(log->open(0));for(unsigned i=0;i<1000;++i)log->feed(rec(i),false,false);
 log->finish(true);waitClose(log);assert(log->rows()==300);
 // Replay completo, anche PAD/LANDED, scrittura a blocchi + coda parziale.
 assert(log->open(7,LogMode::Batch));
 for(unsigned i=0;i<123;++i)log->feed(rec(i),false,i==50);
 log->finish(true);waitClose(log);assert(log->rows()==123 && log->ok());
 const auto& replay=*files[log->name()];assert(replay.size()==sizeof(LogHeader)+123*sizeof(Record));
 for(unsigned i=0;i<123;++i){Record r;memcpy(&r,replay.data()+sizeof(LogHeader)+i*sizeof(Record),sizeof(Record));assert(r.sequenza==i && r.crc==crc32(&r,offsetof(Record,crc)));}
 auto fileCount=files.size();assert(log->open(1,LogMode::Off));
 for(unsigned i=0;i<100;++i)log->feed(rec(i),i==0,i==99);
 log->finish(true);assert(!log->active() && log->rows()==0 && files.size()==fileCount);
 pauseWriter=true;assert(log->open(2));
 for(unsigned i=0;i<600;++i)log->feed(rec(i),i==0,false);
 assert(!log->ok() && log->dropped()>0);log->finish();pauseWriter=false;waitClose(log);
 // RAM assente: niente fallback SRAM/flash, niente file creato.
 auto* missing=new Logger;assert(missing->init());psramAvailable=false;
 fileCount=files.size();assert(!missing->testRam());assert(!missing->open(1,LogMode::Ram));
 assert(files.size()==fileCount);psramAvailable=true;
 assert(log->testRam() && log->ramBytes()==cfg::ramBufferBytes);
 assert(log->open(0,LogMode::Ram));auto writes=writeCalls.load(),flushes=flushCalls.load();
 for(unsigned i=0;i<1000;++i)log->feed(rec(i),false,false);
 log->feed(rec(1000),true,false);
 for(unsigned i=1001;i<1050;++i)log->feed(rec(i),false,false);
 delay(30);assert(writeCalls==writes && flushCalls==flushes && log->flashOps()==0);
 assert(log->ramRows()==350 && log->rows()==0);
 log->feed(rec(1050),false,true);waitClose(log);
 assert(log->ok() && !log->unsaved() && log->rows()==351 && log->flashOps()==0);
 const auto& ramFile=*files[log->name()];
 std::vector<uint8_t> ramCopy(log->ramFileSize());
 assert(log->readRamFile(0,ramCopy.data(),ramCopy.size())==ramCopy.size());
 assert(ramCopy==ramFile);
 for(unsigned i=0;i<351;++i){Record r;memcpy(&r,ramFile.data()+sizeof(LogHeader)+i*sizeof(Record),sizeof(r));assert(r.sequenza==700+i && r.crc==crc32(&r,offsetof(Record,crc)));}
 // STOP in PAD salva solo gli ultimi 300, in ordine.
 assert(log->open(0,LogMode::Ram));for(unsigned i=0;i<1000;++i)log->feed(rec(i),false,false);
 log->finish(true);waitClose(log);assert(log->rows()==300 && log->ok());
 Record first;memcpy(&first,files[log->name()]->data()+sizeof(LogHeader),sizeof(first));assert(first.sequenza==700);
 // Replay: LANDED intermedio non termina la raccolta.
 assert(log->open(1,LogMode::Ram));for(unsigned i=0;i<123;++i)log->feed(rec(i),i==1,i==50);
 assert(log->active() && log->rows()==0);log->finish(true);waitClose(log);assert(log->rows()==123 && log->ok());
 // Salvataggio fallito: copia RAM integra, nuovi RUN e RAMTEST bloccati, SAVE riprova.
 assert(log->open(1,LogMode::Ram));for(unsigned i=0;i<123;++i)log->feed(rec(i),false,false);
 failWrite=true;log->finish();waitClose(log);assert(log->unsaved() && !log->ok());
 assert(!log->open(1,LogMode::Off));assert(!log->testRam());
 ramCopy.resize(log->ramFileSize());log->readRamFile(0,ramCopy.data(),ramCopy.size());
 assert(ramCopy.size()==sizeof(LogHeader)+123*sizeof(Record));
 failWrite=false;assert(log->retrySave());waitClose(log);assert(log->ok() && !log->unsaved());
 assert(*files[log->name()]==ramCopy);
 // CRC prima di salvare: corruzione RAM rilevata e buffer trattenuto.
 assert(log->open(1,LogMode::Ram));log->feed(rec(0),false,false);
 auto* raw=static_cast<uint8_t*>(allocatedRam);raw[0]^=1;log->finish();waitClose(log);
 assert(log->unsaved() && !log->ok());raw[0]^=1;assert(log->retrySave());waitClose(log);assert(log->ok());
 // Buffer pieno: niente sovrascrittura e niente flash, anche oltre durata.
 assert(log->open(1,LogMode::Ram));writes=writeCalls;flushes=flushCalls;
 const unsigned capacity=cfg::ramBufferBytes/sizeof(Record);
 for(unsigned i=0;i<capacity+20;++i)log->feed(rec(i),false,false);
 delay(30);assert(log->active() && log->ramRows()==capacity && log->dropped()==20);
 assert(writeCalls==writes && flushCalls==flushes && log->flashOps()==0);
 log->finish();waitClose(log);assert(!log->ok() && !log->unsaved() && log->rows()==capacity);
 // Limite 180 s sui sensori reali: non salva fino a LANDED.
 assert(log->open(0,LogMode::Ram));writes=writeCalls;flushes=flushCalls;
 log->feed(rec(0),true,false);Record late=rec(1);late.tMs=cfg::maxLogMs;
 log->feed(late,false,false);delay(30);
 assert(log->active() && log->dropped()==1 && writeCalls==writes && flushCalls==flushes);
 log->feed(late,false,true);waitClose(log);assert(log->rows()==1 && !log->ok());
 std::cout<<"PASS RAM: PSRAM obbligatoria, zero flash in acquisizione, PAD circolare, CRC, overflow, limite durata, recupero seriale, SAVE dopo errore\n";
 std::cout<<"PASS logger: PAD senza record flash, prebuffer ordinato, nessun duplicato, chiusura LANDED, snapshot PAD, overflow rilevato\n"<<std::flush;
 std::_Exit(0); // il task FreeRTOS simulato e' intenzionalmente permanente
}
