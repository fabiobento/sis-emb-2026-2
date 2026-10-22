// debounce.h — "debounce" de decisoes de ML (semana 3, agora aplicado a classes).
// Regra: so muda o estado publicado quando N janelas CONSECUTIVAS escolhem a mesma classe
// com confianca >= limiar. Janela incerta (conf < limiar) zera a contagem mas NAO muda o estado.
// C puro, testavel no PC.
#ifndef DEBOUNCE_H
#define DEBOUNCE_H

typedef struct {
    int   estado;     // classe atualmente publicada (-1 = ainda indefinida)
    int   cand;       // classe candidata
    int   cont;       // quantas janelas seguidas confirmaram a candidata
    int   n;          // N exigido (ex.: 3)
    float limiar;     // confianca minima (ex.: 0,80)
} debounce_t;

static inline void debounce_init(debounce_t *d, int n, float limiar)
{ d->estado = -1; d->cand = -1; d->cont = 0; d->n = n; d->limiar = limiar; }

// Alimenta com o resultado de UMA janela. Retorna 1 se o estado publicado mudou.
static inline int debounce_update(debounce_t *d, int classe, float conf)
{
    if (conf < d->limiar) { d->cand = -1; d->cont = 0; return 0; }   // incerto: ignora
    if (classe == d->cand) d->cont++; else { d->cand = classe; d->cont = 1; }
    if (d->cont >= d->n && d->cand != d->estado) { d->estado = d->cand; return 1; }
    return 0;
}
#endif
