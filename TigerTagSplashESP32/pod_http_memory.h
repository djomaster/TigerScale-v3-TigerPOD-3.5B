#pragma once
// Bounded external-memory body. HTTPClient decodes chunk framing before writing.
class PodResponse : public Stream {
public:
    uint8_t* data=nullptr;size_t size=0,capacity=0;bool full=false;
    ~PodResponse(){heap_caps_free(data);}
    int available() override{return 0;}int read() override{return -1;}int peek() override{return -1;}void flush() override{}
    size_t write(uint8_t c) override{return write(&c,1);}
    size_t write(const uint8_t* bytes,size_t n) override {
        const size_t limit=192*1024;
        if(n>limit-size){full=true;return 0;}
        if(size+n>capacity) {
            size_t next=capacity?capacity*2:4096;if(next<size+n)next=size+n;if(next>limit)next=limit;
            auto p=(uint8_t*)heap_caps_realloc(data,next,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            if(!p){full=true;return 0;}data=p;capacity=next;
        }
        if(n)memcpy(data+size,bytes,n);size+=n;return n;
    }
};
static bool r86HttpJson(HTTPClient& http,InventoryJson& doc) {
    PodResponse response;int received=http.writeToStream(&response);
    if(received<0 || response.full || !response.size || (size_t)received!=response.size)return false;
    return !deserializeJson(doc,(const uint8_t*)response.data,response.size,DeserializationOption::NestingLimit(96)) && !doc.overflowed();
}
