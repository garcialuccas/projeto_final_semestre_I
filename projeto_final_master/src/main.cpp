// CÓDIGO DO ESP MAIN

#include <Arduino.h>
#include "internet.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>

// variaveis para o mqtt
const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_client_id = "senai134_esp_main_match_game";
const char *mqtt_topic_sub = "main_match_game_sub";
const char *mqtt_topic_pub = "main_match_game_pub";

// objetos das bibliotecas
WiFiClient espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27, 20, 4);

// estrutura do objeto jogador
struct Jogador {

  String esp; // nome do esp que está enviando as mensagens
  String mensagem; // mensagem enviada
  bool conectado; // conexao do esp sub com o esp main
  bool iniciar; // se o esp sub esta pronto
  float resposta; // resposta que ele envia durante os jogos
  int pontos; // guarda quantos pontos o jogador fez
  String cor; // guarda qual a cor do jogador

};

// quantidade de jogadores, jogadores conectados e jogadores prontos
const int qntdJogadores = 3;
int jogadoresConectados = 0;
int jogadoresProntos = 0;

// variavel para dizer que ninguem se conectou
bool ninguem = 1;

// array para criar e guardar os jogadores
Jogador jogadores[qntdJogadores];

float respostaVerdadeira = 0; // resposta da expressão numerica gerada pelo main
float x, y; // numeros aleatorios gerados a cada pergunta
const int maxPerguntas = 10; // serao feitas 10 perguntas até acabar o jogo
int perguntasFeitas = 0; // guarda quantas perguntas ja foram
int indiceResposta = 0; // variavel para guardar onde a resposta deve ser escrita no lcd

// variaveis para garantir que os numeros gerados serão gerados apenas uma vez, até serem respondidos.
bool gerado = false; 
bool respondido = true;

// variaveis para mostrar a resposta
unsigned long tempoResposta = 0;
bool mostraResposta = false;
bool acertou = false;

// variaveis para mostrar o vencedor
unsigned long tempoVencedor = 0;
bool vencedor = false;

// variavel para iniciar a contagem de jogadores
bool iniciar = true;

// prototipo das funções
void conectaMqtt();
void retornoMqtt(char *, byte *, unsigned int);
void gerarResposta(float n, float m);
void mostrarVencedor();
void telaInicial();

void setup() {

  // gerando a pseudoaleatoriedade
  randomSeed(analogRead(0));

  // colocando os nomes de cada esp em seu respectivo jogador
  jogadores[0].esp = "esp1";
  jogadores[1].esp = "esp2";
  jogadores[2].esp = "esp3";

  // colocando a cor de cada jogador
  jogadores[0].cor = "AZUL";
  jogadores[1].cor = "VERMELHO";
  jogadores[2].cor = "VERDE";

  // configurações da serial
  Serial.begin(115200);

  // inicio do lcd
  lcd.init();
  lcd.backlight();
  telaInicial();

  // funcao para conectar o wifi
  conectaWiFi();

  // funcao para conectar o mqtt
  client.setServer(mqtt_server, mqtt_port);

  // funcao para ler as mensagens recebidas
  client.setCallback(retornoMqtt); 
}

void loop() {
  
  // funcao para checar o wifi
  checkWiFi();

  // atualiza o cliente
  client.loop();

  // garante a conexao continua com o mqtt
  if (!client.connected()) conectaMqtt();

  if (iniciar) {
    jogadoresConectados = jogadores[0].conectado + jogadores[1].conectado + jogadores[2].conectado;
    jogadoresProntos = jogadores[0].iniciar + jogadores[1].iniciar + jogadores[2].iniciar; 
  }

  if ((jogadoresConectados != jogadoresProntos) || ninguem) {
    lcd.setCursor(11, 1);
    lcd.print(jogadoresConectados);
    return;
  }
  else {
    iniciar = false;
    gerado = false;
  }

  if (!gerado) {
    x = random(0, 11);
    y = random(0, 11);
    gerarResposta(x, y);
    gerado = true;
    respondido = false;
  }

  if (!respondido) {

    for (int i = 0; i < qntdJogadores; i++) {
      if (respostaVerdadeira == jogadores[i].resposta) {
        jogadores[i].pontos++;
        perguntasFeitas++;
        acertou = true;
        
        // mostra a resposta asssim que alguem acertar
        lcd.setCursor(0, indiceResposta);
        lcd.print("      ");
        lcd.setCursor(0, indiceResposta);
        lcd.print(respostaVerdadeira);

        // salva o tempo em que a resposta foi acertada
        tempoResposta = millis();
        break; // para sair assim que alguem acertar a respota
      }
    }

    // zerando todas as respostas, para evitar pontos infinitos
    jogadores[0].resposta = 0;
    jogadores[1].resposta = 0;
    jogadores[2].resposta = 0;

    // apos 3 segundos tira a resposta
    if (millis() - tempoResposta >= 3000 && acertou) {
      lcd.clear();
      respondido = true;
      gerado = false;
      acertou = false;
    }
  }

  if (perguntasFeitas == maxPerguntas) {
    mostrarVencedor();
    if (millis() - tempoVencedor >= 10000) {
      tempoVencedor = millis();
      vencedor = !vencedor;
    }

    if (!vencedor) {
      gerado = true;
      respondido = true;
      iniciar = true;
      jogadores[0].iniciar = false;
      jogadores[1].iniciar = false;
      jogadores[2].iniciar = false;
      telaInicial();
      perguntasFeitas = 0;
    }
  }
}

void conectaMqtt() {
  while (!client.connected())
  {
    Serial.println("Conectando ao Mqtt...");
    if (client.connect(mqtt_client_id))
    {
      Serial.println("Conectado");
      client.subscribe(mqtt_topic_sub);
    }
    else
    {
      Serial.print("Falha :");
      Serial.print(client.state());
      Serial.print("Tentando novamente em 5s.");
      delay(5000);
    }
  }
}

void retornoMqtt(char *topic, byte *payload, unsigned int length) {

  Serial.print("Mensagem recebida em: ");
  Serial.print(topic);
  Serial.print(": ");

  String mensagemRecebida = "";
  for (int i = 0; i < length; i++)
  {
    mensagemRecebida += (char)payload[i];
  }

  Serial.print("JSON recebido: ");
  Serial.println(mensagemRecebida);

  JsonDocument doc;
  DeserializationError erro = deserializeJson(doc, mensagemRecebida);

  if (erro)
  {
    Serial.print("Erro ao decodificar JSON: ");
    Serial.print(erro.c_str());
    return;
  }

  // save dos valores recebidos
  const char *esp = doc["esp"];
  const char *msg = doc["msg"];
  const char *conec = doc["conectado"];
  const char *ini = doc["iniciar"];
  const char *resp = doc["resposta"];

  // tratamento e atribuição dos valores recebidos
  for (int i = 0; i < qntdJogadores; i++) {
    if (jogadores[i].esp.equalsIgnoreCase(esp)) {
      jogadores[i].mensagem = msg;
      jogadores[i].resposta = atof(resp);
      if (strcmp(conec, "1") == 0) {
        jogadores[i].conectado = true;
        ninguem = false;
      }
      if (strcmp(ini, "1") == 0) jogadores[i].iniciar = true;
    }
  }
}

// funcao que recebe numeros aletorios, gera uma expressao numerica e devolve a resposta
void gerarResposta(float n, float m) {

  int alternativa = random(0, 4);
  float r = 0;
  String operacoes[] = {"+", "-", "*", "/"};

  lcd.clear();

  if (m == 0 && alternativa == 3) alternativa = random(0, 3);

  else {
    if (alternativa == 0) r = n + m;
    else if (alternativa == 1) r = n - m;
    else if (alternativa == 2) r = n * m;
    else if (alternativa == 3) r = n / m;
  }

  String expressao = String(n) + " " + operacoes[alternativa] + " " + String(m) + " = ??";
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(expressao);

  respostaVerdadeira = r;
  indiceResposta = expressao.indexOf("?");
}

void mostrarVencedor() {

  lcd.clear();
  lcd.setCursor(0, 0);

  int pMax = 0;
  int indice = 0;

  for (int j = 0; j < qntdJogadores; j++) {
    if (jogadores[j].pontos >= pMax) pMax = jogadores[j].pontos;
  }

  for (int i = 0; i < qntdJogadores; i++) {
    if (jogadores[i].pontos == pMax) indice = i;
  }

  lcd.print(jogadores[indice].cor);
  lcd.print(" VENCEU!");
  lcd.setCursor(0, 1);
  lcd.print("PARABENS!");
  
}

void telaInicial() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("INICIAR?");
  lcd.setCursor(0, 1);
  lcd.print("Jogadores: 0");
}