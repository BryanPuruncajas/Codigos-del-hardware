#include <esp_now.h>
#include <WiFi.h>

typedef struct { float params[13]; } ControlInput;
typedef struct { int flag; float values[6]; } ReceivedData;
constexpr size_t MAC_SIZE=6;
constexpr size_t CONTROL_SIZE=sizeof(ControlInput);
constexpr int MAX_FLAGS=16;
float storedData[MAX_FLAGS][6]={{0}};
bool flagReceived[MAX_FLAGS]={false};
uint8_t BROADCAST_MAC[6]={0xff,0xff,0xff,0xff,0xff,0xff};
esp_now_peer_info_t peerInfo{};

void addPeer(const uint8_t* p){
  if(esp_now_is_peer_exist(p)){ Serial.println("OK PEER EXISTS"); return; }
  memset(&peerInfo,0,sizeof(peerInfo)); memcpy(peerInfo.peer_addr,p,6); peerInfo.channel=0; peerInfo.encrypt=false;
  Serial.println(esp_now_add_peer(&peerInfo)==ESP_OK ? "OK PEER ADDED" : "ERR PEER");
}
void removePeer(const uint8_t* p){ Serial.println(esp_now_del_peer(p)==ESP_OK ? "OK PEER REMOVED":"ERR REMOVE"); }
void onDataSend(const uint8_t*,esp_now_send_status_t){}
void onDataReceive(const uint8_t*,const uint8_t* data,int len){
  if(len!=(int)sizeof(ReceivedData)) return;
  ReceivedData r{}; memcpy(&r,data,sizeof(r));
  if(r.flag>=0 && r.flag<MAX_FLAGS){ memcpy(storedData[r.flag],r.values,sizeof(r.values)); flagReceived[r.flag]=true; }
  // Formato único y fácil de parsear: TEL,flag,v0..v5
  Serial.print("TEL,"); Serial.print(r.flag);
  for(int i=0;i<6;i++){ Serial.print(','); Serial.print(r.values[i],6); }
  Serial.println();
}

void setup(){
  Serial.begin(115200); Serial.setTimeout(80); WiFi.mode(WIFI_STA); delay(200);
  Serial.print("BASE_MAC,"); Serial.println(WiFi.macAddress());
  esp_now_deinit();
  if(esp_now_init()!=ESP_OK){ Serial.println("ERR ESPNOW INIT"); return; }
  esp_now_register_send_cb(onDataSend); esp_now_register_recv_cb(onDataReceive); addPeer(BROADCAST_MAC);
}

void loop(){
  while(Serial.available()){
    char type=Serial.read(); uint8_t peer[6];
    if(type=='A') { if(Serial.readBytes(peer,6)==6) addPeer(peer); }
    else if(type=='R') { if(Serial.readBytes(peer,6)==6) removePeer(peer); }
    else if(type=='C') {
      if(Serial.readBytes(peer,6)==6){ ControlInput c{}; if(Serial.readBytes((char*)&c,CONTROL_SIZE)==CONTROL_SIZE) esp_now_send(peer,(uint8_t*)&c,sizeof(c)); }
      Serial.println("OK CONTROL");
    } else if(type=='D') {
      if(Serial.readBytes(peer,6)==6){ delay(5); uint8_t buf[250]; size_t n=min((size_t)Serial.available(),sizeof(buf)); n=Serial.readBytes(buf,n); esp_now_send(peer,buf,n); }
      Serial.println("OK PREF");
    } else if(type=='G') {
      if(Serial.readBytes(peer,6)==6){ uint8_t m[7]; m[0]=0x70; WiFi.macAddress(&m[1]); esp_now_send(peer,m,7); }
      Serial.println("OK GROUND");
    } else if(type=='I') {
      // I + flag byte. Si no llega byte, usa flag 1 por compatibilidad.
      delay(2); int flag=Serial.available()?Serial.read():1;
      if(flag>=0 && flag<MAX_FLAGS && flagReceived[flag]){
        Serial.print("LAST,"); Serial.print(flag);
        for(int i=0;i<6;i++){ Serial.print(','); Serial.print(storedData[flag][i],6); }
        Serial.println();
      } else Serial.println("NO_DATA");
    }
  }
}
