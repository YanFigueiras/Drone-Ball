  // Pinos que chegam da controladora (PWM) A2 DJI
  #define pinoF2 4 // Switch estático com 3 posições
  #define pinoF3 7 // Switch pulsador com 2 posições
  #define pinoTensaoControladora 2 // 5V da controladora

  // Pinos led RGB
  #define pinoLedVermelho 3
  #define pinoLedVerde 5
  #define pinoLedAzul 6

  // Pinos da 1° Ponte H
  #define pinoMotorHorario 9
  #define pinoMotorAntiHorario 10
  #define pinoEnableMotor 11

  // Pinos da 2° Ponte H
  #define pinoAtuadorEstende 12
  #define pinoAtuadorRecolhe 13

  // Usadas em calculaPulso(), tempoPulso armazena a duração do pulso
  // no botão gerada pelo usuário em mili segundos;
  unsigned long tempoInicio = 0;
  unsigned long tempoPulso = 0; 

  // Armazena o estado atual da controladora A2
  // Ligada == TRUE, Desligada == FALSE
  bool estadoControladora;

  // Armazena a duração entre HIGH e LOW do pino e armazena na variável
  // em MICROSEGUNDOS
  int tempoPinoF2 = 0; 
  int tempoPinoF3 = 0; 

  // Estados e variáveis da FSM Atuador Linear ----------------------------
  enum estadosAtuadorLinear {NONE1, RECOLHENDO, RECOLHIDO, ESTENDENDO, ESTENDIDO};
  const unsigned int intervaloFuncionamentoAtuador = 2500; //(2,5s)
  const unsigned int intervaloEsperaAtuador = 45000; //(45s)
  unsigned long tempoInicialAtuador;

  // Estados e variáveis da FSM Motor Esfera ----------------------------
  enum estadosMotorEsfera {NONE2, PARADO, PRENDE, SOLTA};
  int enablePWM = 115; // Define a velocidade do motor esfera

  // Estados e variáveis da FSM Sistema -----------------------------
  enum estadosSistema {NONE3, VERIFICACAO, ARMADO, DESATIVADO};
  const unsigned int intervaloPiscaAmarelo = 150; //(0,15s)
  const unsigned int intervaloPiscaAzul = 300; //(0,3s)
  unsigned long tempoInicialPisca;

  // Variáveis globais que armazenam o estado atual e anterior de cada FSM
  estadosAtuadorLinear estadoAtuadorLinear, estadoAnteriorAtuadorLinear;
  estadosMotorEsfera estadoMotorEsfera, estadoAnteriorMotorEsfera;
  estadosSistema estadoSistema, estadoAnteriorSistema;

  void calculaPulso();
  void atualizaEstadoControladora();
  void FSM_atuadorLinear();
  void FSM_motorEsfera();
  void FSM_sistema();

  void setup() {
    // Configuração dos pinos
    Serial.begin(9600);
    pinMode(pinoF2, INPUT);
    pinMode(pinoF3, INPUT);
    pinMode(pinoEnableMotor, OUTPUT);
    pinMode(pinoMotorHorario, OUTPUT);
    pinMode(pinoMotorAntiHorario, OUTPUT);
    pinMode(pinoAtuadorEstende, OUTPUT);
    pinMode(pinoAtuadorRecolhe, OUTPUT);
    pinMode(pinoLedVermelho, OUTPUT);
    pinMode(pinoLedVerde, OUTPUT);
    pinMode(pinoLedAzul, OUTPUT);

    // Estados iniciais de cada FSM
    estadoAtuadorLinear = ESTENDENDO;
    estadoMotorEsfera = PARADO;
    estadoSistema = VERIFICACAO;

    estadoAnteriorAtuadorLinear = NONE1;
    estadoAnteriorMotorEsfera = NONE2;
    estadoAnteriorSistema = NONE3;
  }

  void loop() {
    // Biblioteca já disponibilizada pelo arduino usada para
    // cronometrar o tempo entre o pino ficar HIGH e LOW 
    // retornando o tempo em microsegundos (1x10^-6 s)
    tempoPinoF2 = pulseInLong(pinoF2, HIGH); 
    tempoPinoF3 = pulseInLong(pinoF3, HIGH); 

    // Caso o switch do pino F3 for pulsado, o tempo do pulso
    // é registrado na variável tempoPulso, e usado posteriormente
    // na FSM do atuador linear
    calculaPulso();
    atualizaEstadoControladora();
    FSM_sistema();
  }

  void FSM_sistema() {
    unsigned long tempoAtual;
    switch (estadoSistema) {
      case VERIFICACAO:
        // Se é a primeira vez entrando no estado faça isso
        if (estadoSistema != estadoAnteriorSistema) { 
          estadoAnteriorSistema = estadoSistema;
          tempoInicialPisca = millis();
          analogWrite(pinoLedVermelho, 255);
          analogWrite(pinoLedVerde, 255);
          analogWrite(pinoLedAzul, 0);
        }
        // Faz essas tarefas nesse estado
        tempoAtual = millis();
        if (tempoAtual >= tempoInicialPisca + intervaloPiscaAmarelo) {
          analogWrite(pinoLedVermelho, 0);
          analogWrite(pinoLedVerde, 0);
          analogWrite(pinoLedAzul, 255);
        } else if (tempoAtual >= tempoInicialPisca + intervaloPiscaAzul){
          analogWrite(pinoLedVermelho, 255);
          analogWrite(pinoLedVerde, 255);
          analogWrite(pinoLedAzul, 0);
          tempoInicialPisca = tempoAtual;
        }
        // Checa se deve mudar de estado
        if ((tempoPinoF2 >= 1200) &&  (tempoPinoF2 <= 1700)) {
          estadoSistema = ARMADO;
        }
      break;

      case ARMADO:
        // Se é a primeira vez entrando no estado faça isso
        if (estadoSistema != estadoAnteriorSistema) { 
          estadoAnteriorSistema = estadoSistema;
          analogWrite(pinoLedVermelho, 255);
          analogWrite(pinoLedVerde, 0);
          analogWrite(pinoLedAzul, 0);
        }
        // Faz essas tarefas nesse estado
        FSM_motorEsfera();
        FSM_atuadorLinear();
        // Checa se deve mudar de estado
        if (!estadoControladora) {
          estadoSistema = DESATIVADO;
        }
      break;

      case DESATIVADO:
        // Se é a primeira vez entrando no estado faça isso
        if (estadoSistema != estadoAnteriorSistema) { 
          estadoAnteriorSistema = estadoSistema;
          analogWrite(pinoLedVermelho, 255);
          analogWrite(pinoLedVerde, 255);
          analogWrite(pinoLedAzul, 0);
        }
        // Checa se deve mudar de estado
        if (estadoControladora) {
          estadoSistema = VERIFICACAO;
        }
      break;
    }
  }

  void FSM_motorEsfera() {
    switch (estadoMotorEsfera){
      case PARADO:
        // Se é a primeira vez entrando no estado faça isso
        if (estadoMotorEsfera != estadoAnteriorMotorEsfera) { 
          estadoAnteriorMotorEsfera = estadoMotorEsfera;
          digitalWrite(pinoMotorHorario, LOW);
          digitalWrite(pinoMotorAntiHorario, LOW);
          analogWrite(pinoEnableMotor, 0);
        }
        // Checa se deve mudar de estado
        if (tempoPinoF2 < 1200) // F2 na posição prender
          estadoMotorEsfera = PRENDE;
        else if (tempoPinoF2 > 1700) // F2 na posição soltar
          estadoMotorEsfera = SOLTA;
      break;

      case PRENDE:
        // Se é a primeira vez entrando no estado faça isso
        if (estadoMotorEsfera != estadoAnteriorMotorEsfera) {
          estadoAnteriorMotorEsfera = estadoMotorEsfera;
          digitalWrite(pinoMotorHorario, HIGH);
          digitalWrite(pinoMotorAntiHorario, LOW);
          analogWrite(pinoEnableMotor, enablePWM);
        }
        // Checa se deve mudar de estado
        if ((tempoPinoF2 >= 1200) &&  (tempoPinoF2 <= 1700)) // F2 na posição prender
          estadoMotorEsfera = PARADO;
      break;

      case SOLTA:
        // Se é a primeira vez entrando no estado faça isso
        if (estadoMotorEsfera != estadoAnteriorMotorEsfera) {
          estadoAnteriorMotorEsfera = estadoMotorEsfera;
          digitalWrite(pinoMotorHorario, LOW);
          digitalWrite(pinoMotorAntiHorario, HIGH);
          analogWrite(pinoEnableMotor, enablePWM);
        }
        // Checa se deve mudar de estado
        if ((tempoPinoF2 >= 1200) &&  (tempoPinoF2 <= 1700)) // F2 na posição prender
          estadoMotorEsfera = PARADO;
      break;
    }
  }

  void FSM_atuadorLinear() {
    unsigned int tempo;
    switch (estadoAtuadorLinear){
      case ESTENDENDO:
        // Se é a primeira vez entrando no estado faça isso
        if (estadoAtuadorLinear != estadoAnteriorAtuadorLinear) { 
          estadoAnteriorAtuadorLinear = estadoAtuadorLinear;
          tempoInicialAtuador = millis();
          digitalWrite(pinoAtuadorEstende, HIGH);
          digitalWrite(pinoAtuadorRecolhe, LOW);
        }
        // Checa se deve mudar de estado
        tempo = millis();
        if (tempo >= tempoInicialAtuador + intervaloFuncionamentoAtuador) 
          estadoAtuadorLinear = ESTENDIDO;
      break;

      case ESTENDIDO:
        // Se é a primeira vez entrando no estado faça isso
        if (estadoAtuadorLinear != estadoAnteriorAtuadorLinear) { 
          estadoAnteriorAtuadorLinear = estadoAtuadorLinear;
          digitalWrite(pinoAtuadorEstende, LOW);
          digitalWrite(pinoAtuadorRecolhe, LOW);
        }
        // Checa se deve mudar de estado
        if (tempoPulso > 2000) { // se o swith for pulsado e durar mais que 2 segundos
          tempoPulso = 0;
          estadoAtuadorLinear = RECOLHENDO;
        }
      break;

      case RECOLHENDO:
        // Se é a primeira vez entrando no estado faça isso
        if (estadoAtuadorLinear != estadoAnteriorAtuadorLinear) { 
          estadoAnteriorAtuadorLinear = estadoAtuadorLinear;
          tempoInicialAtuador = millis();
          digitalWrite(pinoAtuadorEstende, LOW);
          digitalWrite(pinoAtuadorRecolhe, HIGH);
        }
        // Checa se deve mudar de estado
        tempo = millis();
        if (tempo >= tempoInicialAtuador + intervaloFuncionamentoAtuador) 
          estadoAtuadorLinear = RECOLHIDO;
        else if ((tempoPulso > 0) && (tempoPulso < 1000)) { // se o switch for pulsado
          tempoPulso = 0;
          estadoAtuadorLinear = ESTENDENDO;
        }
      break;
        
      case RECOLHIDO:
        // Se é a primeira vez entrando no estado faça isso
        if (estadoAtuadorLinear != estadoAnteriorAtuadorLinear) { 
          estadoAnteriorAtuadorLinear = estadoAtuadorLinear;
          tempoInicialAtuador = millis();
          digitalWrite(pinoAtuadorEstende, LOW);
          digitalWrite(pinoAtuadorRecolhe, LOW);
        }
        // Checa se deve mudar de estado
        tempo = millis();
        if (tempo >= tempoInicialAtuador + intervaloEsperaAtuador) 
          estadoAtuadorLinear = ESTENDENDO;
        else if ((tempoPulso > 0) && (tempoPulso < 1000)) { // se o switch for pulsado
          tempoPulso = 0;
          estadoAtuadorLinear = ESTENDENDO;
        }
      break;
    }
  }

  void calculaPulso() {
    // Ao pulsar o switch --------
    if (tempoPinoF3 >= 1700)
      if (tempoInicio == 0)
        tempoInicio = millis();
    // Ao soltar o switch --------
    if (tempoPinoF3 <= 1200)
      if (tempoInicio != 0) {
        tempoPulso = millis() - tempoInicio;
        tempoInicio = 0;
      }
  }

  void atualizaEstadoControladora() {
    if(digitalRead(pinoTensaoControladora) == HIGH){
      estadoControladora = true;
    }else{
      estadoControladora = false;
    }
  }
