/*
   === BIBLIOTECAS ===
   (lembre‑se de instalá‑las pela “Biblioteca gerenciador” do Arduino IDE)
*/
#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>

/* === CONFIGURAÇÕES === */
#define ID                "equipe40-4"
#define DHTPIN            15          //  *GPIO15 = porta D15*  
#define DHTTYPE           DHT11
#define LED_PIN           2
#define INTERVALO_ENVIO_MS 5000  // 10 segundos em milissegundos
   

/* === STRUCT DE DADOS === */
typedef struct {
  char id[30];
  int dado01;
  int dado02;
  int dado03;
  int dado04;
  int dado05;
} dados_esp;

/* === VARIÁVEIS GLOBAIS === */
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

dados_esp dadosRecebidos;
String     ultimoIDRecebido;
unsigned long ultimoTempoID = 0;
unsigned long ultimoEnvio   = 0;

DHT dht(DHTPIN, DHTTYPE);

/* === FUNÇÕES AUXILIARES === */
void piscarLED(uint16_t tempo = 50) {
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
}

void debugSerial(const dados_esp &d) {
  Serial.println(F("--- Dados Recebidos ---"));
  Serial.printf("ID: %s\n",       d.id);
  Serial.printf("dado01: %d\n",   d.dado01);
  Serial.printf("dado02: %d\n",   d.dado02);
  Serial.printf("dado03: %d\n",   d.dado03);
  Serial.printf("dado04: %d\n",   d.dado04);
  Serial.printf("dado05: %d\n\n", d.dado05);
}

void enviarDados(const dados_esp &pacote, bool piscarLongo = false) {
  esp_now_send(broadcastAddress, (const uint8_t *)&pacote, sizeof(pacote));
  piscarLED(piscarLongo ? 300 : 50);
}

void montarEEnviarInternos() {
  dados_esp dados {};
  strlcpy(dados.id, ID, sizeof(dados.id));

  float temperatura = dht.readTemperature();
  float umidade     = dht.readHumidity();

  dados.dado01 = isnan(temperatura) ? 0 : (int)temperatura;
  dados.dado02 = isnan(umidade)     ? 0 : (int)umidade;
  dados.dado03 = 0;
  dados.dado04 = 0;
  dados.dado05 = 0;

  enviarDados(dados, true);
}

/* === CALLBACKS ESP‑NOW === */
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&dadosRecebidos, incomingData, sizeof(dadosRecebidos));
  debugSerial(dadosRecebidos);

  // Evita eco — só repassa se o pacote não for nosso
  if (String(dadosRecebidos.id) != ID) {
    unsigned long agora = millis();
    if (ultimoIDRecebido != dadosRecebidos.id || (agora - ultimoTempoID > 5000)) {
      ultimoIDRecebido = dadosRecebidos.id;
      ultimoTempoID    = agora;
      enviarDados(dadosRecebidos);
    }
  }
}

void OnDataSent(const uint8_t *, esp_now_send_status_t status) {
  Serial.printf("Pacote enviado, status: %s\n",
                status == ESP_NOW_SEND_SUCCESS ? "SUCESSO" : "FALHA");
}

/* === SETUP === */
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  dht.begin();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("Erro ao iniciar ESP‑NOW"));
    while (true) {}  // trava para facilitar o debug
  }

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  esp_now_peer_info_t peerInfo {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;      // mesmo canal do Wi‑Fi STA
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println(F("Erro ao adicionar peer"));
    while (true) {}
  }

  ultimoEnvio = millis();    // marca para o primeiro envio interno
}

/* === LOOP PRINCIPAL === */
void loop() {
  unsigned long agora = millis();

  if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
    montarEEnviarInternos();
    ultimoEnvio = agora;
  }

  delay(100);  // laço leve
}
