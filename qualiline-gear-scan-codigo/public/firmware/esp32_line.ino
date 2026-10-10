/**
 * QualiLine — ESP32 linha (USB Serial)
 *
 * Ciclo esteira:
 *   - CMD MOTOR OFF → segue MOTOR_OFF_DELAY_MS (identifica / vai ao centro) e só então para
 *   - Após parar, segura MOTOR_OFF_HOLD_MS para a análise da visão
 *   - Ao fim do hold: MOTOR ON AUTOMÁTICO
 *   - Durante delay/hold: MOTOR ON do site é ignorado (não cancela a parada)
 *   - CMD MOTOR HALT → para de verdade (alerta WhatsApp/Telegram), SEM auto ON
 *   - CMD MOTOR ON → libera o HALT e liga
 *   - STATUS FAIL → falhaGuardada=1; SENSOR ON → espera PNEU_ACTUATE_DELAY_MS e aciona válvula
 *     (válvula NÃO para a esteira — motor segue independente)
 *   - CMD PULSE → teste manual do relé (também com delay)
 *
 * Pinos:
 *   23 motor | 32 sensor TTL | 26 TIL 3V3 | 22 relé/válvula
 */

// --- Pinos físicos (GPIO do ESP32) ---
#define PIN_MOTOR         23  // Relé/saída que liga a esteira
#define PIN_SENSOR        32  // Entrada do sensor capacitivo (TTL)
#define PIN_TIL_VCC       26  // Alimentação auxiliar 3V3 do módulo do sensor
#define PIN_PNEU          22  // Relé da válvula/pneumático de rejeito

#define BAUD                 115200  // Velocidade da Serial USB (igual ao site)
/** Defaults do pneumático (podem mudar em runtime via CMD SET PNEU …). */
#define PNEU_PULSE_MS_DEFAULT        750
#define PNEU_ACTUATE_DELAY_MS_DEFAULT 480
/** Identificou a peça: ainda anda este tempo até o centro (ms). */
#define MOTOR_OFF_DELAY_MS   500
/** Tempo parado na análise da visão (ms). */
#define MOTOR_OFF_HOLD_MS    3000
#define PNEU_ACTIVE_HIGH     1  // 1 = HIGH liga a válvula
#define MOTOR_ACTIVE_HIGH    1  // 1 = HIGH liga o motor
#define SENSOR_ACTIVE_LOW    0  // 0 = HIGH significa peça presente
#define SENSOR_DEBOUNCE_MS   20  // Filtra ruído na leitura do sensor (ms)
#define LINE_BUF_SIZE        160  // Tamanho máximo de uma linha serial recebida
#define PNEU_DELAY_MIN_MS    50
#define PNEU_DELAY_MAX_MS    5000
#define PNEU_PULSE_MIN_MS    50
#define PNEU_PULSE_MAX_MS    5000

char lineBuf[LINE_BUF_SIZE];  // Buffer da linha serial atual
size_t lineLen = 0;           // Quantos caracteres já foram lidos

bool lastSensorRaw = false;          // Última leitura bruta do GPIO
unsigned long sensorChangedMs = 0;   // Quando a leitura bruta mudou
bool sensorPresent = false;          // Estado estável após debounce

bool falhaGuardada = false;  // true = visão marcou FAIL; sensor pode acionar válvula

bool motorDesired = true;        // Desejo lógico: esteira ligada?
bool motorPausedByValve = false; // Reservado (hoje a válvula não pausa o motor)

bool motorOffPending = false;           // Aguardando MOTOR_OFF_DELAY_MS antes de parar
unsigned long motorOffAtMs = 0;         // Instantâneo em que deve parar
unsigned long motorOffHoldUntilMs = 0;  // Fim do hold de análise
bool motorHoldActive = false;           // Esteira parada aguardando fim do hold
/** Alerta de lote: motor fica OFF até CMD MOTOR ON (sem auto ON). */
bool motorAlertLock = false;

/** Tempos do pneumático ajustáveis pelo site (aba Pneumática). */
unsigned int pneuActuateDelayMs = PNEU_ACTUATE_DELAY_MS_DEFAULT;
unsigned int pneuPulseMs = PNEU_PULSE_MS_DEFAULT;

// Aplica o nível elétrico do motor conforme flags lógicas.
void applyMotor() {
  bool on = motorDesired && !motorPausedByValve && !motorAlertLock;
  bool level = MOTOR_ACTIVE_HIGH ? on : !on;
  digitalWrite(PIN_MOTOR, level ? HIGH : LOW);  // Liga/desliga GPIO do motor
}

// Cancela um OFF agendado (ainda no delay de aproximação).
void cancelMotorOffDelay() {
  if (motorOffPending) {
    motorOffPending = false;
    Serial.println("MOTOR OFF delay cancelado");
  }
}

// Define se queremos motor ligado e aplica na saída.
void setMotorDesired(bool on) {
  motorDesired = on;
  applyMotor();
  Serial.println(on ? "MOTOR ON" : "MOTOR OFF");
}

/** Para firme para alerta — cancela ciclo delay/hold/autoON. */
void haltMotorAlert() {
  cancelMotorOffDelay();
  motorHoldActive = false;
  motorOffHoldUntilMs = 0;
  motorAlertLock = true;   // Trava até MOTOR ON liberar
  motorDesired = false;
  applyMotor();
  Serial.println("MOTOR HALT (alerta — sem auto ON)");
}

// Agenda parada com delay (peça anda até o centro) + hold de análise.
void requestMotorOffDelayed() {
  if (motorAlertLock) {
    Serial.println("MOTOR OFF ignorado (alerta HALT ativo)");
    return;
  }
  if (motorHoldActive) {
    Serial.println("MOTOR OFF ignorado (ja no hold)");
    return;
  }
  if (motorOffPending) {
    Serial.println("MOTOR OFF ja agendado");
    return;
  }
  if (!motorDesired) {
    // Já estava parado: só inicia o hold de análise.
    motorHoldActive = true;
    motorOffHoldUntilMs = millis() + MOTOR_OFF_HOLD_MS;
    Serial.println("MOTOR OFF (ja parado) → hold 3000ms + auto ON");
    return;
  }

  motorOffPending = true;
  motorOffAtMs = millis() + MOTOR_OFF_DELAY_MS;  // Ainda anda até o centro
  motorDesired = true;
  applyMotor();
  Serial.println("MOTOR OFF em 500ms (identificou → centro)");
}

// Máquina de estados: delay de parada → hold → auto ON.
void atualizaMotorOffDelay() {
  unsigned long now = millis();

  if (motorAlertLock) {
    // Não aplica auto ON nem avança hold de análise.
    if (motorOffPending) {
      motorOffPending = false;
    }
    if (motorHoldActive) {
      motorHoldActive = false;
      motorOffHoldUntilMs = 0;
    }
    motorDesired = false;
    applyMotor();
    return;
  }

  // Chegou a hora de parar de verdade (fim do delay).
  if (motorOffPending && (long)(now - motorOffAtMs) >= 0) {
    motorOffPending = false;
    setMotorDesired(false);
    motorHoldActive = true;
    motorOffHoldUntilMs = now + MOTOR_OFF_HOLD_MS;
    Serial.println("MOTOR OFF aplicado (hold 3000ms analise + auto ON)");
  }

  // Fim do hold: religa a esteira automaticamente.
  if (motorHoldActive && (long)(now - motorOffHoldUntilMs) >= 0) {
    motorHoldActive = false;
    motorOffHoldUntilMs = 0;
    if (motorAlertLock) {
      motorDesired = false;
      applyMotor();
      Serial.println("MOTOR auto ON cancelado (alerta HALT)");
      return;
    }
    if (!motorDesired) {
      setMotorDesired(true);
      Serial.println("MOTOR ON (auto apos hold)");
    } else {
      Serial.println("MOTOR ja ON apos hold");
    }
  }
}

// Liga/desliga a saída do pneumático.
void setPneu(bool on) {
  bool level = PNEU_ACTIVE_HIGH ? on : !on;
  digitalWrite(PIN_PNEU, level ? HIGH : LOW);
}

// Atraso + pulso da válvula (bloqueante com delay — ok para este firmware).
void pulsaValvula() {
  // NÃO mexe no motor — sensor/válvula não param a esteira.
  Serial.print("PNEU DELAY ");
  Serial.print(pneuActuateDelayMs);
  Serial.println("ms (antes de acionar)");
  delay(pneuActuateDelayMs);  // Espera peça chegar à posição do pistão

  setPneu(true);
  Serial.print("PNEU PULSE ");
  Serial.print(pneuPulseMs);
  Serial.println("ms");
  delay(pneuPulseMs);  // Tempo com relé atracado
  setPneu(false);
  Serial.println("PNEU DONE");
}

void printPneuConfig() {
  Serial.print("PNEU CONFIG delay=");
  Serial.print(pneuActuateDelayMs);
  Serial.print(" pulse=");
  Serial.println(pneuPulseMs);
}

/** CMD SET PNEU DELAY <ms> | CMD SET PNEU PULSE <ms> | CMD GET PNEU */
bool trataPneuConfig(const char* line) {
  if (strcmp(line, "CMD GET PNEU") == 0 || strcmp(line, "GET PNEU") == 0) {
    printPneuConfig();
    return true;
  }
  if (strncmp(line, "CMD SET PNEU DELAY ", 19) == 0) {
    int v = atoi(line + 19);
    if (v < (int)PNEU_DELAY_MIN_MS) v = PNEU_DELAY_MIN_MS;
    if (v > (int)PNEU_DELAY_MAX_MS) v = PNEU_DELAY_MAX_MS;
    pneuActuateDelayMs = (unsigned int)v;
    Serial.print("ACK SET PNEU DELAY ");
    Serial.println(pneuActuateDelayMs);
    printPneuConfig();
    return true;
  }
  if (strncmp(line, "CMD SET PNEU PULSE ", 19) == 0) {
    int v = atoi(line + 19);
    if (v < (int)PNEU_PULSE_MIN_MS) v = PNEU_PULSE_MIN_MS;
    if (v > (int)PNEU_PULSE_MAX_MS) v = PNEU_PULSE_MAX_MS;
    pneuPulseMs = (unsigned int)v;
    Serial.print("ACK SET PNEU PULSE ");
    Serial.println(pneuPulseMs);
    printPneuConfig();
    return true;
  }
  return false;
}

// Lê o sensor capacitivo respeitando SENSOR_ACTIVE_LOW.
bool leSensor() {
  int v = digitalRead(PIN_SENSOR);
  if (SENSOR_ACTIVE_LOW) return v == LOW;
  return v == HIGH;
}

// Se há falha guardada e peça no sensor → aciona válvula uma vez.
void tentaAcionarPorSensor(const char* origem) {
  if (!falhaGuardada) return;
  if (!sensorPresent) return;

  Serial.print("VALVULA POR SENSOR (");
  Serial.print(origem);
  Serial.println(") falhaGuardada=1");
  falhaGuardada = false;
  Serial.println("falhaGuardada=0");
  pulsaValvula();
}

// Debounce e publicação SENSOR ON/OFF na Serial.
void atualizaSensor() {
  bool raw = leSensor();
  unsigned long now = millis();
  if (raw != lastSensorRaw) {
    lastSensorRaw = raw;
    sensorChangedMs = now;
  } else if ((now - sensorChangedMs) >= SENSOR_DEBOUNCE_MS) {
    if (sensorPresent != raw) {
      sensorPresent = raw;
      Serial.println(sensorPresent ? "SENSOR ON" : "SENSOR OFF");
      if (sensorPresent) {
        tentaAcionarPorSensor("BORDA");  // Peça acabou de chegar no sensor
      }
    }
  }
}

// Interpreta uma linha completa recebida do site (USB Serial).
void trataLinha(const char* line) {
  // Tempos do pneumático (aba do site)
  if (trataPneuConfig(line)) return;

  // Teste manual do pneumático
  if (strcmp(line, "CMD PULSE") == 0 || strcmp(line, "TEST PNEU") == 0) {
    Serial.println("ACK TEST PULSE");
    pulsaValvula();
    return;
  }

  // Liga esteira (também libera HALT de alerta)
  if (strcmp(line, "CMD MOTOR ON") == 0 || strcmp(line, "MOTOR ON") == 0) {
    if (motorAlertLock) {
      motorAlertLock = false;
      cancelMotorOffDelay();
      motorHoldActive = false;
      motorOffHoldUntilMs = 0;
      setMotorDesired(true);
      Serial.println("MOTOR ON (liberou HALT alerta)");
      return;
    }
    if (motorOffPending || motorHoldActive) {
      Serial.println("MOTOR ON ignorado (ciclo de parada)");
      return;
    }
    setMotorDesired(true);
    return;
  }
  // Parada firme (alerta) — sem auto ON
  if (
    strcmp(line, "CMD MOTOR HALT") == 0 ||
    strcmp(line, "MOTOR HALT") == 0 ||
    strcmp(line, "CMD LINE STOP") == 0
  ) {
    haltMotorAlert();
    return;
  }
  // Ciclo de inspeção: delay + hold + auto ON
  if (strcmp(line, "CMD MOTOR OFF") == 0 || strcmp(line, "MOTOR OFF") == 0) {
    requestMotorOffDelayed();
    return;
  }

  // Demais comandos relevantes começam com "STATUS "
  if (strncmp(line, "STATUS ", 7) != 0) return;
  const char* body = line + 7;

  if (strncmp(body, "EMPTY", 5) == 0) {
    Serial.println("ACK EMPTY");
    if (falhaGuardada) {
      Serial.println("EMPTY: mantem falhaGuardada=1");
    }
    return;
  }

  if (strncmp(body, "OK ", 3) == 0 || strcmp(body, "OK") == 0) {
    falhaGuardada = false;  // Peça boa — não rejeitar
    Serial.println("ACK OK falhaGuardada=0");
    return;
  }

  if (strncmp(body, "FAIL ", 5) == 0 || strcmp(body, "FAIL") == 0) {
    falhaGuardada = true;  // Marca rejeição; sensor dispara a válvula
    Serial.println("ACK FAIL falhaGuardada=1");
    tentaAcionarPorSensor("FAIL");
    return;
  }
}

// Lê bytes da Serial e monta linhas terminadas em \n.
void leSerial() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      if (lineLen > 0) trataLinha(lineBuf);
      lineLen = 0;
      continue;
    }
    if (lineLen + 1 < LINE_BUF_SIZE) lineBuf[lineLen++] = c;
    else lineLen = 0;  // Overflow: descarta e recomeça
  }
}

void setup() {
  pinMode(PIN_MOTOR, OUTPUT);
  pinMode(PIN_PNEU, OUTPUT);
  pinMode(PIN_TIL_VCC, OUTPUT);
  digitalWrite(PIN_TIL_VCC, HIGH);  // Liga alimentação do módulo TIL/sensor
  pinMode(PIN_SENSOR, INPUT_PULLDOWN);

  motorDesired = true;
  motorPausedByValve = false;
  motorOffPending = false;
  motorHoldActive = false;
  motorOffHoldUntilMs = 0;
  motorAlertLock = false;
  setPneu(false);  // Válvula desligada no boot
  applyMotor();

  Serial.begin(BAUD);  // Inicia comunicação serial com o computador/site
  delay(300);
  Serial.println("QUALILINE ESP READY falhaGuardada motor-hold delay500 hold3000 pneuDelay480 pulse750 valveNoStop autoON halt setPneu v18");
  printPneuConfig();

  lastSensorRaw = leSensor();
  sensorPresent = lastSensorRaw;
  sensorChangedMs = millis();
  falhaGuardada = false;

  if (sensorPresent) Serial.println("SENSOR ON");
  setMotorDesired(true);  // Esteira ligada ao iniciar
}

void loop() {
  leSerial();                 // Processa comandos do site
  atualizaMotorOffDelay();    // Avança delay/hold/auto ON
  atualizaSensor();           // Lê e debouca o sensor
  tentaAcionarPorSensor("LOOP");  // Rejeito se falha+sensor
  applyMotor();               // Garante nível elétrico do motor
}
