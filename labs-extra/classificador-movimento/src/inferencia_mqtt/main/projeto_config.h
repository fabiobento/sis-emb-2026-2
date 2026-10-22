// projeto_config.h — TUDO que precisa casar com o Studio e com a rede fica aqui.
#ifndef PROJETO_CONFIG_H
#define PROJETO_CONFIG_H

// ---- Rede (como na semana 14) ----
#define WIFI_SSID   "MINHA_REDE"
#define WIFI_PASS   "MINHA_SENHA"
#define BROKER_URI  "mqtt://192.168.0.10"        // IP do RPi (hostname -I)
#define BANCADA     "bancada1"                   // troque pelo numero da sua bancada
#define T_STATUS    "ifes/" BANCADA "/status"    // LWT: online/offline (retido)
#define T_MOV       "ifes/" BANCADA "/movimento" // {"classe":"aceno","conf":0.93,...}
#define T_INFO      "ifes/" BANCADA "/movimento/info"  // latencias, a cada INFO_A_CADA

// ---- Sensor (IGUAL ao firmware de coleta) ----
#define PIN_SDA    21
#define PIN_SCL    22
#define MPU_ADDR   0x68
#define MPU_RANGE  0     // 0:+-2g 1:+-4g 2:+-8g 3:+-16g
#define MPU_DLPF   3

// ---- Janela (IGUAL ao Impulse no Studio) ----
#define FS_HZ        100          // Studio: Frequency (Hz)
#define JANELA_MS    2000         // Studio: Window size (ms)
#define JANELA_AMOSTRAS (FS_HZ * JANELA_MS / 1000)   // 200
#define STRIDE_MS    500          // so no firmware: de quanto em quanto tempo classificar
#define STRIDE_AMOSTRAS (FS_HZ * STRIDE_MS / 1000)   // 50

// ---- Decisao ----
#define N_CONFIRMA   3            // janelas seguidas iguais para mudar a saida
#define LIMIAR_CONF  0.80f        // confianca minima por janela
#define INFO_A_CADA  20           // publica /info a cada 20 janelas (~10 s)

// ---- Atuador local opcional (semana 8): LED liga quando a classe e CLASSE_ATUA ----
#define PINO_LED     2
#define CLASSE_ATUA  "circulo"

#endif
