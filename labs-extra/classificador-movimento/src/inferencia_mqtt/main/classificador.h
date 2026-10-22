// classificador.h — fronteira entre o seu firmware e o modelo.
//
//   USAR_EI = 1 : usa a biblioteca C++ exportada do Edge Impulse (run_classifier).
//   USAR_EI = 0 : usa um classificador FALSO por energia (so para testar o resto da cadeia:
//                 sensor -> janela -> debounce -> Wi-Fi/MQTT, antes de ter um modelo).
//
// Interface unica:   int clf_classifica(const float *janela, float *prob, float *ms_dsp, float *ms_clf)
//   janela : JANELA_AMOSTRAS*3 floats, x,y,z intercalados, ordem cronologica, em g
//   prob   : CLF_N_CLASSES floats (soma ~1)
//   retorna: indice da classe de maior probabilidade, ou -1 em caso de erro
#ifndef CLASSIFICADOR_H
#define CLASSIFICADOR_H
#include <math.h>
#include <stdio.h>
#include "projeto_config.h"

#ifndef USAR_EI
#define USAR_EI 1
#endif

#if USAR_EI
// ---------------------------------------------------------------------------------------
// Headers gerados pela biblioteca exportada (pastas edge-impulse-sdk/ e model-parameters/).
// Nomes conforme a documentacao do Edge Impulse; confira na SUA versao da biblioteca.
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "model-parameters/model_metadata.h"

// Trava de compilacao: o firmware tem de casar com o Impulse do Studio.
static_assert(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == 3, "o Impulse deve ter 3 eixos (accX,accY,accZ)");
static_assert(EI_CLASSIFIER_RAW_SAMPLE_COUNT == JANELA_AMOSTRAS,
              "janela do Impulse diferente de JANELA_MS*FS_HZ em projeto_config.h");
static_assert((int)EI_CLASSIFIER_FREQUENCY == FS_HZ, "frequencia do Impulse diferente de FS_HZ");

#define CLF_N_CLASSES EI_CLASSIFIER_LABEL_COUNT

static inline const char *clf_nome(int k) { return ei_classifier_inferencing_categories[k]; }

static const float *g_janela_ei;
static int ei_le_dados(size_t off, size_t len, float *out)
{ memcpy(out, g_janela_ei + off, len * sizeof(float)); return 0; }

static inline int clf_classifica(const float *janela, float *prob, float *ms_dsp, float *ms_clf)
{
    g_janela_ei = janela;
    signal_t sinal;
    sinal.total_length = JANELA_AMOSTRAS * 3;
    sinal.get_data = &ei_le_dados;
    ei_impulse_result_t r = { 0 };
    EI_IMPULSE_ERROR e = run_classifier(&sinal, &r, false);
    if (e != EI_IMPULSE_OK) return -1;
    int best = 0;
    for (int k = 0; k < CLF_N_CLASSES; k++) {
        prob[k] = r.classification[k].value;
        if (prob[k] > prob[best]) best = k;
    }
    *ms_dsp = (float)r.timing.dsp;
    *ms_clf = (float)r.timing.classification;
    return best;
}

#else
// ---------------------------------------------------------------------------------------
// Classificador FALSO (stub): decide pela variancia total dos 3 eixos (energia AC do movimento).
// Nao e ML. Serve para validar a cadeia inteira antes do modelo existir.
#define CLF_N_CLASSES 2
static inline const char *clf_nome(int k) { return k == 0 ? "parado" : "movendo"; }

static inline int clf_classifica(const float *j, float *prob, float *ms_dsp, float *ms_clf)
{
    double s[3] = {0, 0, 0}, s2[3] = {0, 0, 0}; int n = JANELA_AMOSTRAS;
    for (int i = 0; i < n; i++)
        for (int e = 0; e < 3; e++) { double v = j[3*i + e]; s[e] += v; s2[e] += v * v; }
    double var = 0;
    for (int e = 0; e < 3; e++) var += s2[e] / n - (s[e] / n) * (s[e] / n);
    double sd = sqrt(var > 0 ? var : 0);            // desvio-padrao total (g)
    float pm = (float)(sd / (sd + 0.05));           // 0..1; 0,5 quando sd = 0,05 g
    prob[0] = 1.0f - pm; prob[1] = pm;
    *ms_dsp = 0; *ms_clf = 0;
    return pm > 0.5f ? 1 : 0;
}
#endif
#endif
