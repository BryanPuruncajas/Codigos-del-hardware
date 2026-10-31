#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "FlowUartParser.h"

// Preserve the broadcast scheme used by the user's working Flowdeck sender.
const uint8_t BLIMP_MAC[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
const uint8_t FLOW_WIFI_CHANNEL=1; // must match blimp AND Ground
const uint32_t UART_BAUD=115200;

char line[121];
char lastUartLine[121]={};
size_t lineLength=0;
bool discardLine=false;
uint32_t sequence=0,validLines=0,badLines=0,queued=0,failed=0;
uint32_t lastReport=0,lastInput=0;
uint32_t uartBytes=0,lastByte=0;
esp_err_t lastSendError=ESP_OK;
FlowProtocol::Packet latest{};

void fatal(const char* operation,esp_err_t error){
    if(Serial)Serial.printf("ERROR %s: %s\n",operation,esp_err_to_name(error));
    while(true)delay(1000);
}

void setup(){
    Serial.begin(115200);
    // The relay must start on battery even when no USB terminal is present.
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(10);
#endif
    Serial1.setRxBufferSize(1024);
    Serial1.begin(UART_BAUD,SERIAL_8N1,D7,D6); // RX44 / TX43, unchanged

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    esp_err_t error=esp_wifi_set_channel(FLOW_WIFI_CHANNEL,WIFI_SECOND_CHAN_NONE);
    if(error!=ESP_OK)fatal("canal WiFi",error);
    error=esp_now_init();
    if(error!=ESP_OK)fatal("esp_now_init",error);
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr,BLIMP_MAC,sizeof(BLIMP_MAC));
    peer.channel=FLOW_WIFI_CHANNEL;
    peer.ifidx=WIFI_IF_STA;
    peer.encrypt=false;
    error=esp_now_add_peer(&peer);
    if(error!=ESP_OK)fatal("esp_now_add_peer",error);

    if(Serial){
        Serial.printf("FLOW-TX-v2 listo. MAC propia=%s canal=%u\n",WiFi.macAddress().c_str(),FLOW_WIFI_CHANNEL);
        Serial.println("Destino broadcast. UART D7: FLOW,seq,dx,dy,z_mm,squal,motion a 115200.");
        Serial.println("ENCOLADO no confirma recepcion; comprobar F6 en la app.");
    }
}

void processLine(){
    if(discardLine){++badLines;return;}
    if(lineLength==0)return;
    line[lineLength]='\0';
    memcpy(lastUartLine,line,lineLength+1);
    FlowProtocol::Packet packet{};
    if(!parseFlowLine(line,sequence,packet)){++badLines;return;}
    ++sequence;++validLines;lastInput=millis();latest=packet;
    // Only real UART samples are sent. Never resend old samples on a timer:
    // otherwise the blimp could see stale measurements as fresh forever.
    const esp_err_t result=esp_now_send(BLIMP_MAC,
        reinterpret_cast<const uint8_t*>(&packet),sizeof(packet));
    lastSendError=result;
    if(result==ESP_OK)++queued;
    else ++failed; // local queuing failure; receiving end still needs checking
}

void loop(){
    // Bounded work keeps heartbeat available even on a continuous UART stream.
    for(unsigned count=0;count<512 && Serial1.available();++count){
        const char c=static_cast<char>(Serial1.read());
        ++uartBytes;lastByte=millis();
        if(c=='\n'){
            processLine();lineLength=0;discardLine=false;
        }else if(c!='\r'){
            if(c=='\0')discardLine=true;
            else if(!discardLine && lineLength<sizeof(line)-1)line[lineLength++]=c;
            else discardLine=true; // discard the entire oversized line
        }
    }
    const uint32_t now=millis();
    if(uint32_t(now-lastReport)>=2000){
        lastReport=now;
        // Preserve the source's actual z field before the invalid-range marker.
        // Diagnostics only: never alter/replay a measurement for this report.
        if(Serial && lastUartLine[0])Serial.printf("UART_RAW: %s\n",lastUartLine);
        if(Serial)Serial.printf("FLOW-TX-v2 bytes=%lu edad_byte=%ld UART=%lu invalidas=%lu encolados=%lu errores_envio=%lu ultimo_error=%d edad_UART=%ld ms seq=%lu dx=%d dy=%d z=%u mm flags=%u calidad=%u\n",
            static_cast<unsigned long>(uartBytes),
            uartBytes?static_cast<long>(uint32_t(now-lastByte)):-1L,
            static_cast<unsigned long>(validLines),static_cast<unsigned long>(badLines),
            static_cast<unsigned long>(queued),static_cast<unsigned long>(failed),
            static_cast<int>(lastSendError),
            validLines?static_cast<long>(uint32_t(now-lastInput)):-1L,
            static_cast<unsigned long>(latest.sequence),
            latest.dx,latest.dy,latest.range_mm,latest.flags,latest.quality);
    }
    delay(1);
}
