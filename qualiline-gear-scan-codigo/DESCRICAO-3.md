# QualiLine — o que é (versão simples)

O QualiLine é um site que roda no seu PC e olha a engrenagem pela câmera.

Ele tenta reconhecer se a peça é o modelo A, B, C ou D.

- Se reconhecer bem → **OK**
- Se não tiver certeza → **FALHA**

Dá pra treinar com fotos boas das peças certas. Não precisa tirar foto de peça ruim.

Se ligar o ESP no USB e apertar **Permitir ESP**, o site manda o resultado pra placa:

- esteira gira
- sensor vê a peça
- se deu FALHA, depois de 1,5 s o pistão rejeita (pino 22)

Tudo fica no computador. Não sobe nada pra internet.

Embaixo da tela de inspeção tem um terminal que mostra o que o ESP está falando, tipo o Serial Monitor do Arduino.
