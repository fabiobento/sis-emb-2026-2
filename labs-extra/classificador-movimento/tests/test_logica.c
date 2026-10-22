// Teste no PC da logica pura do firmware: janela.h (buffer circular + stride) e debounce.h.
//   gcc -Wall -Wextra -o /tmp/test_logica tests/test_logica.c -I src/inferencia_mqtt/main && /tmp/test_logica
#include <stdio.h>
#include <stdlib.h>
#include "janela.h"
#include "debounce.h"
static int falhas = 0;
#define CHECK(c, ...) do { if (!(c)) { falhas++; printf("FALHOU: " __VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    // ---- janela: cap=8, stride=3; amostra n tem valores (n, 10n, 100n) ----
    float mem[8 * EIXOS], out[8 * EIXOS];
    janela_t j; janela_init(&j, mem, 8, 3);
    int prontas = 0; long ult_n = -1;
    for (int n = 1; n <= 20; n++) {
        float a[3] = { n, 10.f * n, 100.f * n };
        if (janela_push(&j, a)) {
            prontas++;
            janela_copia(&j, out);
            // janela mais recente termina na amostra n e tem 8 amostras consecutivas
            for (int i = 0; i < 8; i++) {
                CHECK(out[3*i] == n - 7 + i, "ordem x em n=%d i=%d: %g", n, i, out[3*i]);
                CHECK(out[3*i+1] == 10.f * (n - 7 + i), "ordem y");
                CHECK(out[3*i+2] == 100.f * (n - 7 + i), "ordem z (intercalado)");
            }
            if (ult_n >= 0) CHECK(n - ult_n == 3, "stride entre janelas = %ld", n - ult_n);
            if (prontas == 1) CHECK(n == 8, "primeira janela so quando enche (n=%d)", n);
            ult_n = n;
        }
    }
    // janelas em n = 8, 11, 14, 17, 20
    CHECK(prontas == 5, "numero de janelas = %d (esperado 5)", prontas);

    // ---- debounce: N=3, limiar 0,8 ----
    debounce_t d; debounce_init(&d, 3, 0.8f);
    CHECK(!debounce_update(&d, 1, 0.9f), "1a janela nao muda");
    CHECK(!debounce_update(&d, 1, 0.9f), "2a janela nao muda");
    CHECK( debounce_update(&d, 1, 0.9f) && d.estado == 1, "3a janela confirma classe 1");
    CHECK(!debounce_update(&d, 1, 0.95f), "repeticao nao republica");
    CHECK(!debounce_update(&d, 2, 0.9f), "classe nova: 1/3");
    CHECK(!debounce_update(&d, 1, 0.9f), "volta p/ 1 (ruido): contagem reinicia, estado ja e 1");
    CHECK(d.estado == 1, "estado preservado");
    CHECK(!debounce_update(&d, 2, 0.9f) && !debounce_update(&d, 2, 0.5f), "conf baixa zera a contagem");
    CHECK(!debounce_update(&d, 2, 0.9f) && !debounce_update(&d, 2, 0.9f), "recomecou: 2/3");
    CHECK( debounce_update(&d, 2, 0.9f) && d.estado == 2, "3 seguidas confiantes -> classe 2");

    printf(falhas ? "%d FALHA(S)\n" : "OK: janela.h e debounce.h passaram em todos os testes\n", falhas);
    return falhas != 0;
}
