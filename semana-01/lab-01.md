# Lab 1 — Ambientação: Ubuntu, VS Code, ESP-IDF e o primeiro firmware no Wokwi

> **Antes de começar**: leia a [teoria-01](teoria-01.md) — principalmente a seção 3.1, que
> disseca linha a linha o código que você vai rodar hoje. Este roteiro é autossuficiente:
> cada comando vem com a saída esperada e o que fazer quando ela não aparece.

**Objetivo**: ao final desta prática você terá (a) o ambiente de desenvolvimento completo e
verificado no PC do laboratório; (b) uma conta no Wokwi; (c) seu primeiro firmware ESP32
rodando **no simulador**, com um experimento de temporização documentado.

**Duração**: 2 aulas. **Material**: apenas o PC do laboratório (Ubuntu 24.04). Nenhum
hardware hoje.
**Bancadas**: formem as bancadas de laboratório hoje — elas valem para o semestre (e para o
GitHub).

> 💡 **Por que começar no simulador?** Porque o Wokwi elimina as três variáveis que mais
> atrapalham a primeira semana: cabo, driver e hardware queimado. Se o LED não pisca no
> Wokwi, o problema é **seu código ou sua fiação virtual** — uma variável de cada vez. É o
> mesmo motivo pelo qual a indústria simula antes de mandar fabricar a placa: errar em
> software é grátis; errar em hardware custa semanas.

---

## Parte 0 — Obtendo o código da disciplina (10 min)

Todo o material (teorias, roteiros, códigos) vive no repositório da disciplina no GitHub.
Você fará isto **no início de toda aula prática**, então guarde esta seção.

**1. Primeira vez neste computador** — clone o repositório:

```bash
cd
git clone https://github.com/fabiobento/sis-emb-2026-2.git          
```

**2. Computador que já tem o repositório** — sincronize com a versão oficial da semana:

```bash
cd ~/sis-emb-2026-2
git fetch
git reset --hard origin/main
```

> **Atenção:** durante as práticas é esperado (e recomendável!) que vocês editem os códigos
> para testar hipóteses. A sequência acima baixa as novidades e **sobrescreve** qualquer
> alteração local, garantindo que seu ambiente comece idêntico ao roteiro oficial de hoje.
> Se quiser preservar experimentos seus, copie-os antes para uma pasta fora do repositório
> (ou para o repositório **da bancada**, que vocês criarão na Parte C).

**O que esses comandos fazem?**

- `git fetch` — consulta o GitHub e baixa silenciosamente as novidades, sem alterar seus
  arquivos.
- `git reset --hard origin/main` — força os arquivos locais a ficarem idênticos à versão
  oficial, descartando modificações residuais de aulas anteriores.

---

## Parte A — Ambiente de desenvolvimento (50 min)

Siga [`docs/instalacao.md`](https://github.com/fabiobento/sis-emb-2026-2/blob/main/docs/instalacao.md), seções 1–3. Os marcos de verificação (não avance sem cada ✔):

**A.1 — ESP-IDF v5.2 instalado e ativável.**

```bash
get_idf
idf.py --version
```

Saída esperada:

```
ESP-IDF v5.2.x
```

> **Observação:** `get_idf` é um *alias* criado pela instalação que carrega as variáveis de
> ambiente do ESP-IDF no terminal atual. Ele precisa ser executado **em todo terminal novo**
> em que você for usar `idf.py`. Esquecê-lo é a causa nº 1 de "comando não encontrado" nas
> próximas semanas. Se quiser nunca mais pensar nisso, a instalação oferece colocar o
> `get_idf` no seu `~/.bashrc` — nos PCs do laboratório, isso já está feito.

**A.2 — VS Code com a extensão ESP-IDF** (ícone da Espressif na barra lateral) abrindo sem
erros.

**A.3 — Conta no Wokwi** criada (wokwi.com — pode usar a conta GitHub). A versão web é
gratuita e é a que vocês usarão; a extensão paga do VS Code é usada pelo professor na
preparação.

**A.4 — Conta no GitHub** de cada integrante da bancada (se ainda não tiver).

---

## Parte B — Primeiro firmware no Wokwi (50 min)

### B.1 Criando o projeto

1. Em [wokwi.com/esp32](https://wokwi.com/esp32), vá até a seção **ESP-IDF Templates** e escolha **ESP32**. Assim você escolherá o template **ESP-IDF** (não o Arduino!). Agora você verá dois painéis: o editor de código à esquerda e o circuito à direita, com uma placa ESP32 DevKit.
2. Abra `~/sis-emb-2026-2/semana-01/src/blink_wokwi/main.c` no seu PC, copie **todo** o conteúdo e
   cole sobre o `main.c` do Wokwi. Este é exatamente o código dissecado na seção 3.1 da
   teoria — se pulou, volte lá: você precisa saber o que cada linha faz.

### B.2 Montando o circuito

3. No painel do circuito, clique no **+** e adicione as peças: `LED` e
   `RESISTOR`.
4. Ligue: pino **D2** do ESP32 → resistor → **anodo (A)** do LED; **catodo (C)** do LED →
   **GND**. Clique no resistor e defina o valor: **220** (ohms). *Por que 220 Ω? É o Exemplo
   resolvido 3.2 da semana 3 — por ora, aceite; em duas semanas você fará essa conta de
   olhos fechados.*

   ![](https://raw.githubusercontent.com/fabiobento/sis-emb-2026-2/main/assets/figuras/lab-01.png)
   

> 🔌 **Regra do LED que vale para o semestre inteiro**: LED tem polaridade. O **anodo**
> (perna mais longa) vai para o lado positivo (o GPIO, via resistor); o **catodo** (perna
> curta, face chanfrada no corpo) vai para o GND. Invertido, ele simplesmente não acende —
> não queima, não explode, só finge que não é com ele. No Wokwi, a perna dobrada é o anodo.

5. Alternativa rápida: cole no arquivo `diagram.json` do projeto:

```json
{
  "version": 1,
  "author": "sua-bancada",
  "editor": "wokwi",
  "parts": [
    {
      "type": "board-esp32-devkit-c-v4",
      "id": "esp",
      "top": -67.2,
      "left": 43.24,
      "attrs": { "builder": "esp-idf" }
    },
    {
      "type": "wokwi-led",
      "id": "led1",
      "top": 44.4,
      "left": -24.6,
      "attrs": { "color": "red", "flip": "1" }
    },
    {
      "type": "wokwi-resistor",
      "id": "r1",
      "top": 120,
      "left": -77.35,
      "rotate": 90,
      "attrs": { "value": "220" }
    }
  ],
  "connections": [
    [ "esp:TX", "$serialMonitor:RX", "", [] ],
    [ "esp:RX", "$serialMonitor:TX", "", [] ],
    [ "r1:1", "led1:A", "green", [ "v-19.2" ] ],
    [ "led1:C", "esp:GND.1", "green", [ "v0" ] ],
    [ "r1:2", "esp:2", "green", [ "v18", "h220.8", "v-76.8" ] ]
  ],
  "dependencies": {}
}
```
> Nota: Você também pode encontrar uma cópia desse circuito nesse projeto Wokwi: [Lab-01](https://wokwi.com/projects/471428016666162177).

### B.3 Rodando e observando

6. Clique em **▶**. Comportamento esperado: o LED da protoboard **e** o LED azul embutido da
   placa piscam juntos a 1 Hz (500 ms aceso, 500 ms apagado), e o monitor serial na parte de
   baixo imprime:

```
LED = 1
LED = 0
LED = 1
```

Se o LED da protoboard não piscar mas o log aparecer: problema de **fiação** (confira anodo
× catodo). Se nada acontecer: o código não compilou — leia a mensagem de erro no console
(hábito que vale ouro: a primeira linha do erro, não a última, costuma ser a causa).

### B.4 Experimentos (registre tudo para o relatório)

7. **Experimento 1 — período**: mude `vTaskDelay(pdMS_TO_TICKS(500))` para **200 ms** e
   rode. O pisca fica visivelmente mais rápido? E com **20 ms**? Você ainda percebe o
   piscar ou o LED parece continuamente aceso (mais fraco)? Anote o menor período em que
   seu olho ainda distingue o piscar — esse número reaparecerá na semana 8 (PWM e a
   frequência de cintilação: o olho "integra" piscadas rápidas demais, e é exatamente esse
   defeito da visão que o PWM explora para criar níveis de brilho).
8. **Experimento 2 — assimetria**: faça o LED ficar 900 ms aceso e 100 ms apagado (dica:
   dois `vTaskDelay` diferentes dentro do laço, um após cada `gpio_set_level`).
9. **Experimento 3 — atraso ocupado (leia, pense, responda)**: se você trocasse o
   `vTaskDelay` por `for (volatile long i = 0; i < 10000000; i++);`, o LED continuaria
   piscando — mas o que mudaria "por dentro"? Use o Exemplo resolvido 1.1 da teoria para
   argumentar em termos de consumo de energia. (No Wokwi a diferença não aparece; num nó a
   bateria, é a diferença entre 10 meses e 25 horas.)

> 🧠 **Dica de relatório**: o Experimento 3 é a pergunta-conceito mais importante da semana.
> A resposta completa tem três camadas: (1) o laço `for` mantém a CPU a 100 % executando
> instruções inúteis — energia jogada fora; (2) `vTaskDelay` **bloqueia a tarefa** e devolve
> a CPU ao sistema (que pode dormir ou trabalhar em outra tarefa); (3) em firmware real,
> desperdiçar CPU é desperdiçar bateria — software define consumo.

---

## Parte C — Repositório da bancada (20 min)

1. Um integrante cria no GitHub o repositório **`sis-emb-2026-bancada-XX`** (XX = número da
   bancada), privado, e adiciona o colega e o professor como colaboradores.
2. Estrutura mínima: uma pasta por lab (`lab-01/`, `lab-02/`, …), cada uma com
   `relatorio.md`.
3. Commit de hoje: `lab-01/relatorio.md` com o conteúdo da entrega abaixo.

> **Observação:** o histórico de commits ao longo do semestre é critério de avaliação do
> projeto final (20 % — documentação). Commits pequenos e frequentes, com mensagens
> descritivas, desde já. Mensagem boa: "lab01: experimento 2 — LED assimétrico 900/100 ms".
> Mensagem ruim: "update".

---

## 🛠️ Problemas comuns (consulte antes de chamar o professor)

| Sintoma | Causa provável | Remédio |
|---|---|---|
| `idf.py: command not found` | esqueceu o `get_idf` | rode `get_idf` no terminal |
| Wokwi não compila | erro de C apontado no console | leia a **primeira** linha do erro |
| LED da placa pisca, o da protoboard não | LED invertido ou fio no pino errado | anodo↔D2, catodo↔GND |
| Nada pisca e sem log | projeto errado (template Arduino) | recrie com template ESP-IDF |
| "Failed to connect" no Wokwi | — (não existe: é hardware real) | esse erro é do Lab 2! |

## Entrega (via GitHub da bancada, até a próxima aula prática)

1. **Link do projeto Wokwi** funcionando (botão *Share* → *Copy link*) + captura de tela.
2. Tabela do Experimento 1: período testado × "piscar perceptível? (sim/não)".
3. Código do Experimento 2 (só o trecho do laço).
4. Resposta ao Experimento 3: qual é o papel de `vTaskDelay()` e por que um laço `for`
   vazio é uma péssima ideia num sistema a bateria? (3–6 linhas, citando o Exemplo 1.1.)
5. Print do `idf.py --version` do PC da bancada (prova de que a Parte A foi concluída).

## Desafio (opcional, +0,5 no relatório)

Faça o LED piscar em **código Morse** a palavra "SOS" (··· ––– ···) em loop: ponto = 200 ms,
traço = 600 ms, pausa entre símbolos = 200 ms, entre letras = 600 ms. Dica: crie uma função
`void simbolo(int duracao_ms)` para não repetir código.
