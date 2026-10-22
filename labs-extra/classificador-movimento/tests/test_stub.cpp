// Testa o classificador FALSO (USAR_EI=0) e a cadeia janela -> classificador -> debounce no PC.
//   g++ -Wall -DUSAR_EI=0 -o /tmp/test_stub tests/test_stub.cpp -I src/inferencia_mqtt/main -lm && /tmp/test_stub
#include <cstring>
#include "classificador.h"
#include "janela.h"
#include "debounce.h"
int main()
{
    static float mem[JANELA_AMOSTRAS * EIXOS], jan[JANELA_AMOSTRAS * EIXOS];
    janela_t j; janela_init(&j, mem, JANELA_AMOSTRAS, STRIDE_AMOSTRAS);
    debounce_t d; debounce_init(&d, N_CONFIRMA, LIMIAR_CONF);
    int mudancas = 0, ok = 1; float prob[CLF_N_CLASSES], t1, t2;
    // 10 s parado (ruido 0,005 g), depois 10 s sacudindo (senoide 3 Hz, 0,5 g)
    for (int n = 0; n < 2000; n++) {
        float t = n / (float)FS_HZ, ruido = 0.005f * ((n * 7919) % 100 - 50) / 50.f;
        float amp = (n < 1000) ? 0.f : 0.5f;
        float a[3] = { amp * sinf(2 * 3.14159f * 3 * t) + ruido, ruido, 1.f + ruido };
        if (janela_push(&j, a)) {
            janela_copia(&j, jan);
            int c = clf_classifica(jan, prob, &t1, &t2);
            if (debounce_update(&d, c, prob[c])) {
                mudancas++; printf("t=%.1fs -> %s (%.2f)\n", t, clf_nome(d.estado), prob[c]);
                if (mudancas == 1 && strcmp(clf_nome(d.estado), "parado")) ok = 0;
                if (mudancas == 2 && strcmp(clf_nome(d.estado), "movendo")) ok = 0;
            }
        }
    }
    printf(ok && mudancas == 2 ? "OK: 2 transicoes (parado -> movendo)\n" : "FALHOU (%d mudancas)\n", mudancas);
    return !(ok && mudancas == 2);
}
