// janela.h — janela deslizante (buffer circular) para o classificador de movimento.
// C puro, sem dependencia de ESP-IDF: testavel no PC (tests/test_logica.c).
//
//   push(amostra) ──▶ [ . . . . . . . . . . ]  buffer circular com 'cap' amostras
//                        ▲ head (proxima escrita)
//   a cada 'stride' amostras novas, com o buffer ja cheio, push() devolve 1:
//   "ha uma janela nova para classificar". janela_copia() entrega a janela em ORDEM
//   CRONOLOGICA (mais antiga primeiro), intercalada x,y,z,x,y,z,... — o layout que o
//   Edge Impulse espera em EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME = 3.
#ifndef JANELA_H
#define JANELA_H
#include <string.h>

#define EIXOS 3

typedef struct {
    float *dados;       // cap * EIXOS floats (fornecido pelo chamador)
    int    cap;         // amostras por janela (ex.: 200 = 2 s a 100 Hz)
    int    stride;      // a cada quantas amostras novas gera-se uma janela (ex.: 50 = 0,5 s)
    int    head;        // indice (em amostras) da proxima escrita
    long   total;       // amostras recebidas desde o inicio
    int    desde;       // amostras recebidas desde a ultima janela entregue
} janela_t;

static inline void janela_init(janela_t *j, float *mem, int cap, int stride)
{
    j->dados = mem; j->cap = cap; j->stride = stride;
    j->head = 0; j->total = 0; j->desde = 0;
    memset(mem, 0, sizeof(float) * cap * EIXOS);
}

// Retorna 1 quando uma nova janela esta pronta (buffer cheio E passou o stride).
static inline int janela_push(janela_t *j, const float a[EIXOS])
{
    memcpy(&j->dados[j->head * EIXOS], a, sizeof(float) * EIXOS);
    j->head = (j->head + 1) % j->cap;
    j->total++;
    j->desde++;
    if (j->total >= j->cap && j->desde >= j->stride) { j->desde = 0; return 1; }
    return 0;
}

// Copia a janela atual em ordem cronologica para dst[cap * EIXOS].
static inline void janela_copia(const janela_t *j, float *dst)
{
    int n1 = j->cap - j->head;                       // amostras do head ate o fim do vetor
    memcpy(dst, &j->dados[j->head * EIXOS], sizeof(float) * n1 * EIXOS);
    memcpy(dst + n1 * EIXOS, j->dados, sizeof(float) * j->head * EIXOS);
}
#endif
