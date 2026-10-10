# Descrição do projeto — QualiLine

O QualiLine é um sistema local de inspeção visual de engrenagens. Por meio de câmera e classificação automática, identifica se a peça corresponde a um dos quatro modelos de referência (A, B, C ou D), indicando aprovação (OK) ou rejeição (FALHA).

O treinamento utiliza apenas imagens de peças conformes. Com um ESP32 conectado por USB, a decisão da visão comanda a esteira e o acionamento pneumático de rejeição. O processamento ocorre no computador da bancada, sem envio de dados à nuvem.
