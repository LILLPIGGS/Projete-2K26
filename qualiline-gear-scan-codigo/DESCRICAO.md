# Descrição do projeto — QualiLine

## O que é

O **QualiLine** é um sistema de inspeção visual de engrenagens que roda no computador local. Uma câmera observa a peça na linha; o software classifica se ela corresponde a um dos **quatro modelos** treinados (A, B, C ou D) e decide **OK** ou **FALHA**. Quando há um ESP32 ligado por USB, a decisão da visão comanda a esteira e a rejeição pneumática.

O processamento e os dados ficam no PC: nada é enviado para a nuvem.

## Problema que resolve

Em uma linha de montagem ou bancada, peças fora de especificação ou de modelo errado não podem seguir. A inspeção manual é lenta e inconsistente. O QualiLine automatiza essa verificação com visão computacional e, opcionalmente, atua na linha (motor e pistão) para separar as peças rejeitadas.

## Como funciona

1. A câmera captura frames em tempo real na página web (Chrome/Edge).
2. O motor de visão (ONNX no navegador, ou modo demo sem treino) analisa a imagem.
3. Se a confiança e a margem forem suficientes para um dos modelos A–D, a peça é **OK**; caso contrário, **FALHA** (não reconhecida / não encaixa).
4. Contadores e gráficos registram a produção da sessão.
5. Com o ESP conectado (**Permitir ESP**), o PC envia linhas `STATUS OK`, `STATUS FAIL` ou `STATUS EMPTY` pela USB Serial (115200).
6. O ESP gira o motor da esteira, lê o sensor capacitivo e, em caso de FAIL, espera o sensor + 1,5 s e pulsa a válvula no GPIO 22.

O treino usa apenas fotos **boas** dos quatro modelos e fotos de fundo vazio — não é necessário fotografar defeitos.

## Componentes

| Parte | Função |
|-------|--------|
| Site Next.js (QualiLine) | Interface, câmera, IA no navegador, dataset, treino, terminal serial |
| Modelos ONNX + `manifest.json` | Classificador, embedder de anomalia e limiares por modelo |
| Firmware ESP32 (`esp32_line.ino`) | Motor (GPIO 23), sensor TTL (GPIO 32), TIL 3V3 (GPIO 26), pneumática (GPIO 22) |
| Sensor capacitivo + relé + pistão | Detecção física da peça e rejeição mecânica |

## Fluxo na linha

1. Peça entra; a esteira gira (motor via MOSFET no GPIO 23).
2. A câmera classifica; após ~250 ms estáveis, o PC manda `STATUS` ao ESP.
3. **OK** — a peça só passa; rejeição pendente é limpa.
4. **FAIL** — o ESP guarda a rejeição; quando o sensor (GPIO 32) indica presença (ou se já estava ativo), aguarda 1,5 s e aciona o relé/válvula no GPIO 22.
5. Mesa vazia → `STATUS EMPTY` e o ciclo reinicia.

Um terminal na própria página de inspeção espelha o que apareceria no Serial Monitor do Arduino (mensagens do ESP e STATUS enviados pelo PC).

## Tecnologias

- **Frontend:** Next.js, React, TypeScript, Tailwind
- **Visão:** ONNX Runtime Web (classificador + anomalia por protótipos)
- **Hardware:** ESP32, Web Serial API, sensor capacitivo TTL, MOSFET, relé pneumático
- **Treino:** Python (scripts em `training/`), acionável pelo botão **Treinar agora** no site

## Escopo e limitações

- Funciona em **localhost** no PC onde estão a câmera e o ESP (Web Serial não opera por túnel remoto).
- A qualidade da inspeção depende do dataset, da iluminação e da calibração dos limiares em `manifest.json`.
- Sem **Permitir ESP**, a visão e os contadores seguem normais; só a atuação da linha fica desligada.
