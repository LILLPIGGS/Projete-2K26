# Descrição do projeto — QualiLine (versão 2)

## Identificação

O projeto **QualiLine** (também referido como Engrena) consiste no desenvolvimento de uma estação de inspeção automatizada de engrenagens, integrando visão computacional em tempo real e atuação eletropneumática controlada por microcontrolador. O objetivo é classificar peças em quatro modelos de referência e impedir que unidades não conformes prossigam na linha.

## Objetivo geral

Garantir, de forma automática e rastreável na sessão de operação, que apenas engrenagens reconhecidas com confiança suficiente como pertencentes aos modelos A, B, C ou D sejam consideradas aprovadas, rejeitando as demais por meio de decisão visual e, quando o hardware estiver conectado, por desvio mecânico na linha.

## Objetivos específicos

- Capturar e analisar imagens da peça sob a câmera sem depender de serviços em nuvem.
- Classificar a peça entre os modelos treinados ou declará-la não reconhecida (falha).
- Permitir coleta de dataset e retreinamento no próprio ambiente do operador.
- Integrar um ESP32 à decisão da visão para comandar motor de esteira, leitura de sensor de presença e acionamento de válvula pneumática de rejeição.
- Exibir na interface o espelhamento do monitoramento serial, facilitando diagnóstico da linha.

## Justificativa

A inspeção exclusivamente humana é sujeita a fadiga e variação entre operadores. Um sistema local, com limiares configuráveis e atuação física acoplada à classificação, reduz o risco de peças incorretas seguirem adiante e documenta OK/FALHA ao longo da operação.

## Metodologia (visão do sistema)

O software é uma aplicação web (Next.js) executada no computador da bancada. O navegador acessa a câmera, executa o modelo ONNX e aplica critérios de confiança, margem entre classes e, no modo completo, verificação de anomalia por protótipos. A decisão estável gera o envio de comandos `STATUS` ao ESP32 via Web Serial.

No firmware, o ciclo típico é: esteira em movimento; recebimento de OK ou FAIL; em FAIL, armazenamento da rejeição; detecção da peça pelo sensor capacitivo; atraso de 1,5 s; pulso no GPIO da válvula. Peças aprovadas atravessam sem acionamento do pistão.

## Resultados esperados

- Indicação visual imediata de OK ou FALHA na interface.
- Contagem percentual de peças certas e erradas na sessão.
- Rejeição física coerente com a classificação quando ESP, sensor e pneumática estiverem operacionais.
- Capacidade de recalibrar limiares e retreinar a partir de novas fotos boas dos quatro modelos.

## Público e ambiente de uso

Operadores e técnicos em bancada ou célula de inspeção, com PC Windows/Linux (Chrome ou Edge), câmera USB ou integrada, e opcionalmente ESP32 com MOSFET, sensor TTL e relé pneumático. O uso do ESP exige abertura do site em `localhost` no mesmo computador da USB.
