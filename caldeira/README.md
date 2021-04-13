java -jar aquecedor2008_1.jar 6565
gcc -pthread -o udp udpcliente2008.c && ./udp localhost 6565

# Control
H in (0.1, 3)m
T = Tref

 Sensors:
  - Ta - temperatura do ar ambiente em volta do recipiente [Grau Celsius] 
  - T - temperatura da água no interior do recipiente [Grau Celsius]
  - Ti - temperatura da água que entra no recipiente [Grau Celsius] 
  - No - fluxo de água de saída do recipiente [Kg/segundo] 
   -H - altura da coluna de água dentro do recipiente [m]

Actuators:
  - Q -  fluxo de calor do elemento aquecedor [Joule/segundo]
  - Ni fluxo de água de entrada do recipiente [Kg/segundo]
  - Na fluxo de água aquecida a 80C de entrada controlada [Kg/segundo]
  - Nf fluxo de água de saída para esgoto controlada [Kg/segundo]

Disturbances
  - No
  - Ta
  - Ti

