#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// HSU normal frame + exact ACK. No fixed 10-ms ready polling; the caller still
// consumes and validates the complete response. POD only, normal weighing unchanged.
namespace PodUart {
template<class Port> bool ack(Port& port,const uint8_t* cmd,size_t n,uint32_t timeout=100) {
    if(!n || n>32)return false;
    uint8_t frame[40]={0,0,0xff,(uint8_t)(n+1),(uint8_t)(0-(n+1)),0xd4};
    uint8_t sum=0xd4;for(size_t i=0;i<n;i++){frame[6+i]=cmd[i];sum+=cmd[i];}
    frame[6+n]=(uint8_t)(0-sum);frame[7+n]=0;
    if(port.write(frame,n+8)!=n+8)return false;
    const uint8_t expected[]={0,0,0xff,0,0xff,0};uint8_t reply[6];size_t got=0;
    uint32_t start=millis();
    while(got<sizeof(reply) && (uint32_t)(millis()-start)<timeout) {
        while(port.available() && got<sizeof(reply)){int b=port.read();if(b>=0)reply[got++]=(uint8_t)b;}
        if(got<sizeof(reply))delay(1);
    }
    return got==sizeof(reply)&&!memcmp(reply,expected,sizeof(reply));
}
}
