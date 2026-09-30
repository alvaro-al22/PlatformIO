# USB Lab: laboratorio educativo con ESP32-S3

Este proyecto convierte un ESP32-S3 en la base de un laboratorio modular controlado desde el ordenador. No necesitas pantalla, botones externos ni bateria: un cable USB proporciona alimentacion y comunicacion.

El firmware incluye consola, diagnostico y controladores para Wi-Fi, BLE, PN532, CC1101, IR, GPIO, RFID de 125 kHz, iButton, microSD y USB HID. **Implementado no significa probado en hardware:** los perifericos externos todavia no se han conectado para validar esta ampliacion. No es un clon completo de Flipper Zero; la tabla siguiente y los comandos delimitan las funciones disponibles.

El proyecto esta pensado para aprender con dispositivos propios y en un entorno controlado. No incluye funciones de ataque, clonacion de tarjetas de terceros ni captura de credenciales.

## Indice

- [1. Conceptos basicos](#1-conceptos-basicos)
- [2. Que funciona actualmente](#2-que-funciona-actualmente)
- [3. Material y conexiones](#3-material-y-conexiones)
- [4. Instalacion en Windows](#4-instalacion-en-windows)
- [5. Compilar, flashear y abrir la consola](#5-compilar-flashear-y-abrir-la-consola)
- [6. Utilizar los comandos](#6-utilizar-los-comandos)
- [7. Entender los diagnosticos y logs](#7-entender-los-diagnosticos-y-logs)
- [8. Como esta organizado el proyecto](#8-como-esta-organizado-el-proyecto)
- [9. Configuracion y reserva de pines](#9-configuracion-y-reserva-de-pines)
- [10. Problemas habituales](#10-problemas-habituales)
- [11. Como continuar el desarrollo](#11-como-continuar-el-desarrollo)
- [12. Estado de las pruebas](#12-estado-de-las-pruebas)

## 1. Conceptos basicos

| Termino | Explicacion |
|---|---|
| ESP32-S3 | El microcontrolador: el chip que ejecuta el programa y controla los perifericos. |
| Placa de desarrollo | La placa que contiene el microcontrolador, conectores USB, regulador de alimentacion y pines accesibles. |
| Firmware | El programa que se instala en la placa. Sigue guardado aunque desconectes el USB. |
| Compilar | Convertir el codigo fuente en un archivo que el ESP32 puede ejecutar. Se hace en el ordenador. |
| Flashear | Escribir ese programa en la memoria flash de la placa. Sustituye el firmware anterior. |
| Flash | Memoria que conserva el programa al apagar. En N16R8 se esperan 16 MB. |
| RAM / PSRAM | Memoria de trabajo temporal. N16R8 incluye 8 MB de PSRAM externa, ademas de la RAM interna del chip. |
| CLI | Interfaz de linea de comandos: escribes una instruccion y recibes una respuesta de texto. |
| Puerto COM | Nombre que Windows asigna a una conexion serie, por ejemplo `COM5`. No es un numero fijo. |
| Monitor serie | Programa del ordenador que permite enviar y recibir texto por el puerto COM. |
| GPIO | Pin del microcontrolador que puede servir como entrada, salida o senal de un periferico. |
| Driver | Codigo que sabe comunicarse con un componente concreto; requiere el modelo, interfaz y cableado adecuados. |
| Log | Mensaje del firmware que informa de lo que ocurre o ayuda a localizar errores. |
| Heap | Parte de la memoria de trabajo que el programa puede solicitar y liberar mientras funciona. |

Se utiliza **ESP-IDF**, el framework oficial de Espressif, porque ya proporciona consola, gestion de tareas y herramientas de diagnostico. **PlatformIO** descarga y coordina el framework, el compilador y las herramientas de grabacion. No son alternativas entre si: aqui PlatformIO gestiona un proyecto basado en ESP-IDF. No se utiliza Arduino.

## 2. Que funciona actualmente

| Parte | Estado actual |
|---|---|
| Comunicacion USB mediante el puente USB-UART | Implementada en el firmware. |
| Consola y menu de texto | Implementados. |
| Informacion del sistema | Version, chip, tiempo encendido, motivo de reinicio y memoria disponible. |
| Diagnostico basico | Comprueba chip, tamanos de memoria, integridad del heap y reservas de pines. |
| Wi-Fi | Escaneo activo/pasivo por canal, auditoria, referencias persistentes, conexion y PMF individual experimental. Reinicios anteriores sin resolver. |
| Bluetooth LE | Escaneo pasivo, conexion central y GATT: servicios, caracteristicas, lectura y escritura confirmada. |
| NFC / PN532 | Informacion del lector, UID ISO14443A y lectura/escritura limitada a NTAG213/215/216 identificadas. |
| CC1101 | RX/TX de paquetes FSK de hasta 32 bytes con presets de banda, potencia -10 dBm y transmision confirmada. |
| IR | Aprendizaje y repeticion de pulsos, portadora configurable, guardado/carga en NVS. |
| GPIO | Entrada, salida y un canal PWM, solo GPIO7/15/16/47. |
| RFID 125 kHz | Lectura de ID EM4100 mediante un lector RDM6300 UART; no escritura ni emulacion. |
| iButton | Busqueda de ROM de 64 bits por 1-Wire, con CRC del controlador; no escritura ni emulacion. |
| microSD | Montaje FAT por SPI, listar/leer/crear/borrar archivos; no formateo automatico. |
| USB HID | Teclado ASCII con distribucion US, tecla individual y movimiento de raton por USB nativo; activacion explicita. |

Wi-Fi y Bluetooth no se inicializan al arrancar: solo se encienden al usar `wifi` o `ble`. Las credenciales Wi-Fi se guardan solo en RAM y se pierden al reiniciar. No hay pantalla, interfaz grafica, bateria, PCB propia ni carcasa.

Las referencias Wi-Fi y senales IR se guardan en NVS solo mediante sus comandos explicitos. Los buses externos, salidas y USB HID no se activan desde los diagnosticos. Si NVS esta llena o es incompatible, se devuelve un error: **no se borra automaticamente**. Guardar datos y transmitir consumen recursos aunque la compilacion haya sido correcta; valida cada periferico por separado.

La CLI **se ejecuta en el ESP32**. El ordenador solo actua como terminal. El menu es una lista de comandos: no se navega con botones ni seleccionando numeros.

## 3. Material y conexiones

### Lo necesario ahora

- ESP32-S3 DevKitC-1 N16R8.
- Cable USB compatible con la placa y capaz de transferir datos.
- Ordenador con Windows y acceso a Internet para la instalacion inicial.

La variante N16R8 tiene 16 MB de flash y 8 MB de PSRAM octal. El ESP32-S3 tiene dos nucleos y puede alcanzar 240 MHz; eso no significa que este proyecto lo configure necesariamente a esa frecuencia maxima.

### Perifericos opcionales

Sin modulos puedes compilar y utilizar la consola y diagnosticos. No necesitas comprar todo para probar la placa. Antes de conectar cada periferico, verifica su modelo y niveles electricos con la tabla de la seccion 9. Los comandos de lectura sin modulo pueden devolver timeout o error; no son una prueba de presencia fiable. No ejecutes transmisiones con una antena o etapa de salida desconocida.

### Elegir el conector USB correcto

La DevKitC-1 dispone de un puerto **USB-to-UART** y otro conectado al **USB nativo del ESP32-S3**. Identificalos por las etiquetas de tu placa o por su documentacion, no por su posicion: las variantes pueden diferir.

**La consola usa el puerto USB-to-UART.** El USB nativo se reserva para HID, activado solo con `usb start confirm`, y no proporciona esta consola. Antes de conectar ambos puertos a la vez, comprueba el esquema y las rutas de alimentacion de tu placa; no unas fuentes que puedan realimentarse.

```text
PowerShell / monitor serie en el ordenador
                  |
              Cable USB
                  |
      Puente USB-UART de la placa
                  |
    UART0 del ESP32-S3, GPIO43 y GPIO44
                  |
          Consola del firmware
                  |
        Registro de modulos
                  |
    system / wifi / ble / nfc / cc1101 / ir / gpio
                     rfid / ibutton / sd / usb
```

La comunicacion usa **115200 baudios, 8 bits de datos, sin paridad, 1 bit de parada y sin control de flujo**: suele abreviarse como 115200, 8N1. Estos ajustes ya estan preparados en el proyecto.

**Seguridad electrica:** los GPIO trabajan a 3,3 V y no toleran 5 V. Verifica consumo, reguladores, masa comun y conversion de niveles antes de alimentar modulos. Desconecta todas las fuentes antes de cambiar cableado. Un emisor IR requiere una etapa de potencia adecuada; no conectes un LED de potencia directamente al GPIO.

## 4. Instalacion en Windows

### Si estas usando este mismo ordenador y carpeta

Ya se preparo un entorno local `.venv` y se instalo PlatformIO. Puedes pasar a la seccion 5. Los pasos siguientes sirven para repetir la instalacion en otro ordenador o recuperar el entorno.

### Preparar las herramientas

1. Instala [Visual Studio Code](https://code.visualstudio.com/).
2. Instala [Python para Windows](https://www.python.org/downloads/windows/). La compilacion inicial se realizo con Python 3.10.11. Si utilizas otra version, puede haber diferencias de compatibilidad con las herramientas fijadas del proyecto. En el instalador, habilita la opcion de anadir Python al PATH.
3. Instala [Git para Windows](https://git-scm.com/downloads/win), necesario para las herramientas de desarrollo de ESP-IDF.
4. Reinicia VS Code si estaba abierto durante la instalacion.
5. En VS Code, selecciona **Archivo > Abrir carpeta** y abre la carpeta que contiene este README y la configuracion del proyecto.
6. Abre **Terminal > Nuevo terminal**. Los comandos de esta guia usan PowerShell.

Comprueba que las herramientas estan disponibles:

```powershell
python --version
git --version
```

Comprueba tambien que estas en la carpeta correcta:

```powershell
Get-Location
Test-Path .\platformio.ini
```

El ultimo comando debe devolver `True`. Si devuelve `False`, cambia a la carpeta del proyecto antes de continuar. Por ejemplo, en el ordenador donde se creo:

```powershell
Set-Location "C:\Users\alvar\Desktop\Programacion\PlatformIO"
```

En otro ordenador, utiliza la ruta real de tu carpeta.

### Crear el entorno e instalar PlatformIO

Ejecuta estos comandos uno por uno. Si alguno falla, resuelve el error antes de continuar:

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install platformio==6.1.18
.\.venv\Scripts\pio.exe --version
```

`.venv` es una carpeta con herramientas de Python aisladas para este proyecto. No se copia a la placa. No hace falta activar el entorno ni cambiar la politica de ejecucion de PowerShell: los comandos llaman directamente a sus ejecutables.

No necesitas instalar Arduino IDE ni instalar ESP-IDF manualmente. La extension PlatformIO de VS Code es opcional: esta guia utiliza la herramienta de linea de comandos instalada en `.venv`.

## 5. Compilar, flashear y abrir la consola

**Hay dos lugares distintos donde escribir comandos:**

| Donde | Que se escribe |
|---|---|
| PowerShell, antes de abrir el monitor serie | Comandos que empiezan por `.\.venv\Scripts\pio.exe`. |
| Consola de la placa, cuando aparece `lab>` | Comandos como `menu` o `system info`. |

No escribas `system info` directamente en PowerShell ni intentes ejecutar comandos de PlatformIO dentro de `lab>`.

### Paso 1: compilar

No hace falta conectar la placa para este paso:

```powershell
.\.venv\Scripts\pio.exe run -e esp32s3_n16r8
```

`-e esp32s3_n16r8` selecciona la configuracion de compilacion del proyecto. La primera vez se descargan herramientas y puede tardar varios minutos; necesitas conexion a Internet y espacio libre en disco. Las siguientes compilaciones suelen ser mas rapidas.

El gestor de componentes descarga las dependencias de [src/idf_component.yml](src/idf_component.yml). [dependencies.lock](dependencies.lock) fija las versiones resueltas: PN532 0.2.1, RadioLib 7.8.1, onewire_bus 1.1.2, esp_tinyusb 1.7.6~2 y TinyUSB 0.21.0~2. No edites las copias generadas en `managed_components/`. El HAL ESP-IDF de RadioLib requiere `CONFIG_FREERTOS_HZ=1000`; esta configuracion y `CONFIG_TINYUSB_HID_COUNT=1` estan habilitadas en los valores iniciales y la configuracion efectiva.

Al terminar debe aparecer `SUCCESS`. Si aparece `FAILED`, no continues al flasheo: lee el primer error del proceso.

El firmware se genera en [firmware.bin](.pio/build/esp32s3_n16r8/firmware.bin). PlatformIO tambien genera el cargador de arranque y la tabla de particiones, y sabe donde grabar cada archivo. No necesitas elegir direcciones de memoria manualmente.

### Paso 2: conectar y localizar el puerto

Conecta el cable al puerto **USB-to-UART** de la placa y ejecuta:

```powershell
.\.venv\Scripts\pio.exe device list
```

Busca el puerto correspondiente a la placa, por ejemplo `COM5`. Si hay varios, compara la lista con la placa desconectada y conectada. Tambien puedes consultar **Administrador de dispositivos > Puertos (COM y LPT)** en Windows.

En los siguientes comandos, **sustituye `COM5` por tu puerto real**.

### Paso 3: flashear

Este paso sustituye el firmware existente. Cierra cualquier monitor serie que este usando el puerto y ejecuta:

```powershell
.\.venv\Scripts\pio.exe run -e esp32s3_n16r8 -t upload --upload-port COM5
```

No desconectes el cable mientras se escribe la memoria. Al finalizar, PlatformIO normalmente reinicia la placa.

Si se queda intentando conectar, puedes utilizar los botones BOOT y RESET que ya incorpora la placa: manten BOOT, pulsa y suelta RESET, y suelta BOOT. Repite el comando de flasheo. No necesitas comprar ni conectar botones externos. Si queda en modo descarga al acabar, pulsa RESET sin mantener BOOT.

### Paso 4: abrir el monitor serie

```powershell
.\.venv\Scripts\pio.exe device monitor -e esp32s3_n16r8 --port COM5
```

El monitor utiliza la velocidad configurada en el proyecto. Pulsa RESET para ver el arranque completo si ya habia ocurrido antes de abrir el monitor. Despues de los mensajes, deberia aparecer:

```text
lab>
```

Escribe `menu` y pulsa Enter. No escribas el prefijo `lab>`: lo muestra la placa.

Para salir del monitor y volver a PowerShell, pulsa **Ctrl+C**. Si necesitas flashear otra vez, cierra primero el monitor para liberar el puerto COM.

### Uso diario

Si no has cambiado el codigo y la placa ya tiene este firmware, basta con conectarla y abrir el monitor serie. No hace falta reinstalar herramientas, compilar ni flashear cada vez. La flash conserva el firmware; los datos temporales de RAM y el historial de la consola se pierden al reiniciar.

## 6. Utilizar los comandos

Los comandos se escriben en minusculas y se ejecutan con Enter.

### Sistema y consola

| Comando | Que hace |
|---|---|
| `help` | Muestra la ayuda de los comandos registrados. |
| `menu` | Lista los modulos y su estado de implementacion. |
| `system info` | Muestra version, chip, tiempo encendido, motivo de reinicio y memoria. |
| `system diag` | Repite el diagnostico del sistema. |
| `system pins` | Muestra las reservas de GPIO; no activa ni modifica esos pines. |

### Wi-Fi

| Comando | Que hace |
|---|---|
| `wifi scan [active\|passive] [canal]` | Redes de 2,4 GHz: BSSID, SSID, RSSI, canal, autenticacion y cifrados. Maximo 20 resultados; modo activo por defecto. Canal 0=todos los permitidos; argumento 0..13, sujeto al pais configurado. No captura PMF. Ejemplo: `wifi scan passive 6`. |
| `wifi audit` | Avisa de redes abiertas, WEP, WPA antiguo o cifrados WEP/TKIP. No captura PMF, prueba contrasenas ni confirma la identidad del router. |
| `wifi pmf <BSSID>` | Consulta experimental del PMF anunciado por una red del ultimo escaneo, de menos de 120 segundos. Requiere estar desconectado y escucha hasta 5 segundos en su canal. Pendiente de validar en placa. |
| `wifi trust <BSSID>` | Guarda un punto de acceso del ultimo escaneo como referencia conocida. Usa el formato `aa:bb:cc:dd:ee:ff`. |
| `wifi trust list` | Muestra las referencias conocidas guardadas en RAM. |
| `wifi trust clear` | Borra las referencias de RAM, no las guardadas en NVS. |
| `wifi trust save` | Sustituye la copia NVS por la lista actual de RAM. No guarda credenciales. |
| `wifi trust load` | Recupera explicitamente la lista NVS; no se carga automaticamente al arrancar. |
| `wifi trust erase` | Borra solo la copia persistente; deja la RAM intacta. |
| `wifi suspects` | Escanea y marca BSSID desconocidos con SSID conocido, o cambios de SSID/seguridad en un BSSID registrado. Es una alerta para revisar, no una confirmacion de ataque. |
| `wifi connect <ssid> [password]` | Conecta a una red. Si el nombre tiene espacios usa comillas: `wifi connect "Mi Red" clave123`. |
| `wifi status` | Muestra si esta conectado, la red, la senal y la IP. |
| `wifi disconnect` | Se desconecta de la red. |

La lista `trust` admite hasta 16 puntos de acceso. Sin `save`, los cambios se pierden al reiniciar; usa `load` para recuperar una copia previa. El formato persistente esta ligado a esta version de firmware/SDK y comprueba version, longitud y tamano de registro; no es un formato de intercambio portable. Registra los nodos legitimos de una red mesh o repetidores para evitar alertas esperadas. El inventario solo cubre los resultados retenidos y no determina por si solo si una red es segura.

#### PMF anunciado

**Estado actual: consulta individual experimental con `wifi pmf <BSSID>`.** Las pruebas anteriores en placa provocaron reinicios al activar el modo promiscuo, incluso con el callback en IRAM. Separar la captura del escaneo no garantiza resolver ese fallo. `wifi scan`, `wifi audit` y `wifi suspects` no activan ni detienen el modo promiscuo, ni muestran campos PMF o `SKIP PMF`.

Tras compilar y flashear, puedes probarlo en la consola `lab>` con una red propia:

```text
wifi disconnect
wifi scan
wifi pmf aa:bb:cc:dd:ee:ff
```

Sustituye la direccion por el BSSID de un resultado retenido. La consulta exige un escaneo correcto de menos de 120 segundos y rechaza una estacion conectada; no desconecta automaticamente ni realiza otro escaneo. Escucha hasta 5 segundos en el canal guardado y termina antes si recibe un anuncio coincidente. Si el punto de acceso ha cambiado de canal, repite `wifi scan`.

Al terminar, tambien ante errores de captura devueltos por el SDK, desactiva el modo promiscuo, retira el callback y restaura el filtro y el canal anteriores. Si la limpieza falla, bloquea nuevos comandos de radio hasta reiniciar. El limite de escucha no puede impedir un bloqueo o reinicio dentro del driver. No desactives el watchdog.

PMF (Protected Management Frames, IEEE 802.11w) protege determinadas tramas de gestion, incluidas las de desconexion, frente a falsificaciones cuando se negocia en la conexion. No evita interferencias de radio ni impide conectarse a quien conoce la contrasena.

El campo `pmf_advertised` del comando individual tiene los siguientes significados:

| Valor | Significado |
|---|---|
| `unsupported` | El anuncio observado no declara soporte PMF: MFPC desactivado o sin elemento RSN. |
| `optional` | El punto de acceso declara soporte PMF, pero no lo exige (MFPC=1, MFPR=0). |
| `required` | El punto de acceso exige PMF (MFPC=1, MFPR=1). |
| `unknown` | Sin anuncio capturado dentro del plazo, o primer anuncio incompleto/incoherente. No significa que PMF este desactivado en el router. |

Leer estos anuncios no requiere conectarse a la red ni conocer su contrasena. El callback solo filtra y copia el primer beacon o respuesta de sondeo cuyo BSSID, direccion de origen y canal coincidan con el objetivo; descarta recepciones con errores y anuncios de mas de 1536 bytes. El analizador se ejecuta despues de detener la captura. No se agregan observaciones ni se detectan contradicciones entre varios anuncios.

La consulta PMF es una escucha pasiva en 2,4 GHz. El escaneo habitual puede enviar solicitudes de sondeo. No se capturan credenciales ni contenido del trafico de los clientes. El analizador usa los bits MFPC/MFPR del elemento RSN, no una deduccion a partir de WPA2/WPA3. Los anuncios no prueban la identidad del emisor.

**Es PMF anunciado, no el PMF negociado por cada cliente.** El contador de advertencias de `wifi audit` sigue contando solo redes abiertas/antiguas y cifrados obsoletos, no estados PMF. `wifi suspects` y `trust` no muestran, guardan ni comparan PMF; siguen comparando BSSID, SSID, autenticacion y cifrados. No buscan dispositivos conectados a tu router.

### Bluetooth LE

| Comando | Que hace |
|---|---|
| `ble scan [segundos]` | Escucha anuncios Bluetooth LE durante 1 a 30 segundos; usa 5 segundos por defecto. No se conecta a los dispositivos. |
| `ble status` | Estado local de BLE y conexion. |
| `ble connect <MAC> public\|random` | Conecta al dispositivo propio indicado, usando el tipo de direccion anunciado. Plazo de conexion de 10 s, espera de CLI hasta 12 s y limpieza posterior. |
| `ble disconnect` | Solicita desconexion; espera hasta 3 s. |
| `ble services` | Descubre hasta 64 servicios de la conexion actual. |
| `ble chars <inicio> <fin>` | Descubre hasta 64 caracteristicas entre handles decimales. |
| `ble read <handle>` | Una lectura ATT; muestra como maximo 64 bytes. No es una lectura larga completa. |
| `ble write <handle> <hex> confirm` | Escribe hasta 20 bytes con respuesta del dispositivo. Respeta sus permisos. |

Cada operacion GATT espera hasta 10 s. Tras timeout se solicita cerrar la conexion y se bloquean nuevas operaciones hasta su cierre; si el cierre falla, puede ser necesario reiniciar. No se implementan emparejamiento interactivo, notificaciones, advertising ni Bluetooth Classic. Los dispositivos que requieran autenticacion pueden rechazar estas operaciones.

### NFC, RFID e iButton

| Comando | Operacion y limites |
|---|---|
| `nfc info` | Consulta firmware del PN532 en modo I2C. |
| `nfc scan` | Busca una tarjeta ISO14443A a 106 kbps y muestra UID; espera de seleccion 1 s. |
| `nfc read <pagina>` | Lee una pagina de NTAG213/215/216 identificada; permite paginas iniciales y de usuario. |
| `nfc write <pagina> <8hex> confirm` | Escribe 4 bytes y comprueba mediante lectura. Solo paginas de usuario 4..39/129/225 segun modelo; excluye UID, bloqueo y configuracion. |
| `rfid read [segundos]` | Espera de 1 a 30 s, por defecto 5; acepta tramas RDM6300 con checksum valido. |
| `ibutton scan` | Busca hasta 16 ROM 1-Wire; identifica familia 01 como DS1990A. |

Una escritura NFC fallida no garantiza que la tarjeta siga intacta. Usa etiquetas propias prescindibles. No hay lectura universal NFC, autenticacion MIFARE Classic/DESFire, edicion NDEF, escritura LF ni emulacion de tarjetas o iButton.

### Radio CC1101

| Comando | Operacion y limites |
|---|---|
| `cc1101 status` | Estado del controlador. |
| `cc1101 init EU433\|EU868\|US915` | Configura 433,92/868,35/915 MHz; FSK 4,8 kbps, desviacion 5 kHz, ancho RX 58 kHz y potencia -10 dBm. |
| `cc1101 rx [segundos]` | Espera un paquete de hasta 32 bytes, durante 1..10 s; 5 s por defecto. |
| `cc1101 tx <hex> confirm` | Envia un paquete de hasta 32 bytes; intervalo minimo de 60 s entre transmisiones. |
| `cc1101 sleep` | Duerme el transceptor; necesita `init` antes de volver a operar. |

Los nombres de presets no certifican cumplimiento normativo. Comprueba banda, antena, licencia y reglas locales antes de transmitir. RX/TX vuelven a standby. No es un analizador de espectro ni captura/reproduccion de mandos OOK o codigos variables; otro equipo debe usar parametros de paquete compatibles.

### Infrarrojos

| Comando | Operacion y limites |
|---|---|
| `ir learn [segundos]` | Captura pulsos con receptor demodulado activo-bajo; espera 1..30 s, por defecto 5. Descarta la senal anterior al iniciar. |
| `ir show` | Muestra la senal en RAM y su portadora configurada. |
| `ir carrier <kHz>` | 30..60 kHz; por defecto 38. El receptor no mide la portadora. |
| `ir send confirm` | Reproduce una vez la senal; espera de transmision hasta 10 s. |
| `ir list` | Lista senales guardadas en NVS. |
| `ir save <nombre> confirm` | Guarda o sustituye una senal en NVS. |
| `ir load <nombre>` | Carga una senal compatible. |
| `ir remove <nombre> confirm` | Borra una senal NVS. |

Capacidad: menos de 256 simbolos RMT (hasta dos duraciones por simbolo), resolucion 1 us, filtro minimo 100 us y fin de captura por pausa de 15 ms. No todos los protocolos caben; una captura que llena el buffer se rechaza. No hay decodificador de protocolos, importacion de archivos Flipper ni deteccion de portadora. Las senales se guardan en NVS, no en microSD.

### GPIO y microSD

| Comando | Operacion y limites |
|---|---|
| `gpio pins` | Lista GPIO7/15/16/47; todos los demas estan protegidos. |
| `gpio mode <pin> in\|out\|pullup\|pulldown` | Configura el pin; salida inicialmente baja. |
| `gpio read <pin>` | Lee el nivel; rechaza un pin con PWM activo. |
| `gpio write <pin> 0\|1` | Requiere salida configurada previamente. |
| `gpio pwm <pin> <Hz> <porcentaje>` | Un canal, 1..40000 Hz, 0..100 %, resolucion de 10 bits. |
| `gpio release <pin>` | Detiene PWM si procede y libera el pin. |
| `sd mount` / `sd status` / `sd unmount` | Monta FAT por SPI a 4 MHz, consulta estado o desmonta; nunca formatea automaticamente. |
| `sd list` | Lista hasta 64 entradas de la raiz. |
| `sd read <nombre>` | Muestra hasta 4096 bytes; escapa bytes no imprimibles. |
| `sd write <nombre> <texto> confirm` | Crea un archivo nuevo, nunca sobrescribe uno existente. Texto con espacios entre comillas. |
| `sd remove <nombre> confirm` | Borra el archivo indicado. |

Los nombres de archivos SD y senales IR admiten 1..15 caracteres ASCII alfanumericos, guion o guion bajo. No admiten rutas, espacios, puntos ni extensiones; no es un explorador de archivos general. La microSD debe estar previamente formateada en FAT compatible; desmontala antes de retirarla. La disponibilidad de nombres largos depende de la configuracion FatFS del firmware.

### USB HID

| Comando | Operacion y limites |
|---|---|
| `usb status` | Estado del controlador USB/HID. |
| `usb start confirm` | Instala HID de teclado y raton en USB nativo. No se activa al arrancar. |
| `usb stop confirm` | Desinstala el controlador. |
| `usb text <ASCII> confirm` | Escribe texto imprimible, con mapa de teclado US; no anade Enter. |
| `usb key <uso> confirm` | Pulsa y libera un uso HID decimal 4..115, sin modificadores. |
| `usb mouse <dx> <dy> confirm` | Movimiento relativo -127..127 por eje, sin botones. |

Usa solo un ordenador propio y una ventana de prueba: el texto llega a la aplicacion con foco. Una distribucion distinta de US puede producir otros caracteres. Los informes de teclado incluyen liberacion de teclas; ante desconexion o estado pendiente se bloquean nuevos envios. Si queda bloqueado, usa `usb stop confirm` y reinicia el controlador. VID/PID 0xcafe/0x4011 son identificadores de desarrollo, no una asignacion comercial. No hay scripts, ejecucion automatica al conectar, almacenamiento USB ni motor DuckyScript.

Prueba inicial sin perifericos, una linea cada vez dentro de la consola:

```text
help
menu
system info
system diag
system pins
wifi trust list
gpio pins
sd status
usb status
cc1101 status
ir show
```

Los comandos incompletos como `nfc` muestran su uso. Un timeout con un lector desconectado no demuestra una averia. Prueba Wi-Fi separadamente segun la seccion 12, porque existe un historial de reinicios sin resolver.

Si escribes solo `system`, recibiras:

```text
ERR USAGE: system info|diag|pins
```

La barra vertical significa "elige una opcion". Debes escribir, por ejemplo, `system info`, no las tres opciones juntas. Un comando desconocido debe producir un error sin reiniciar la placa.

La consola usa la implementacion oficial de ESP-IDF. Tiene configurados un limite de linea de 128 y un historial de 16 entradas. La disponibilidad de edicion avanzada depende del soporte de la terminal; el uso basico es escribir una orden y pulsar Enter.

## 7. Entender los diagnosticos y logs

### Que comprueba `system diag`

- Que el chip sea un ESP32-S3 de dos nucleos.
- Que la flash detectada tenga 16.777.216 bytes, equivalentes a los 16 MB esperados.
- Que la PSRAM este inicializada y tenga 8.388.608 bytes, equivalentes a los 8 MB esperados.
- Que las estructuras de gestion del heap sean coherentes y haya al menos 32 KiB de heap interno libre.
- Que la lista de pines reservados no tenga duplicados ni utilice pines excluidos por el proyecto.
- Si el motivo del ultimo reinicio corresponde a determinados errores, como caida de alimentacion, panic o watchdog, muestra una advertencia.

Un watchdog es un mecanismo que detecta que el programa lleva demasiado tiempo sin responder. Un brownout es una caida de tension que puede provocar un reinicio. El motivo de reinicio se muestra como un codigo numerico de ESP-IDF, no como una descripcion completa.

El diagnostico tambien se ejecuta automaticamente durante el arranque de la aplicacion, antes de abrir la consola.

| Mensaje | Significado |
|---|---|
| `OK` | La comprobacion concreta ha pasado. No certifica toda la placa. |
| `ERR` | Un comando o una comprobacion ha fallado. Lee el detalle que lo acompana. |
| `SKIP` | Esa comprobacion no se ha realizado. No equivale a que el hardware funcione. |
| `software available` en el menu | El modulo tiene implementacion; no es el resultado de una prueba electrica. |
| `I (...)` | Log informativo. |
| `W (...)` | Advertencia que conviene revisar. |
| `E (...)` | Log de error. |

Es normal ver lineas `SKIP` para los perifericos: el diagnostico no activa sus buses ni realiza transmisiones. Si las comprobaciones implementadas pasan, el resumen sera:

```text
OK implemented checks passed; SKIP is not PASS
```

### Lo que no puede diagnosticar todavia

No detecta si has conectado un PN532, CC1101 o modulo IR. No comprueba antenas, cableado, cortocircuitos, tensiones de GPIO ni calidad de una senal. Comprobar reservas de pines solo valida la configuracion del programa.

La comprobacion de flash verifica su tamano, no todos sus sectores. La integridad del heap comprueba su gestion interna, no es un ensayo exhaustivo de toda la RAM. ESP-IDF tiene ademas habilitada su prueba de PSRAM durante el arranque.

Si el diagnostico de la aplicacion falla, esta intenta mantener disponible la consola. Sin embargo, un fallo anterior, durante el arranque de ESP-IDF, puede impedir llegar a ella. La configuracion permite continuar si la PSRAM no se encuentra cuando ESP-IDF puede recuperarse de ese caso, pero no garantiza recuperacion de cualquier fallo de memoria.

Respuestas y logs comparten la misma conexion. Es una consola para personas, **no un protocolo JSON ni una API automatizada** con identificadores de peticion. Los logs no se guardan automaticamente en un archivo del ordenador ni en la flash.

## 8. Como esta organizado el proyecto

Los archivos `.c` contienen implementaciones en C; los `.h` contienen declaraciones y configuracion compartida. El adaptador RadioLib utiliza C++ con entrada compatible con C.

| Archivo | Responsabilidad |
|---|---|
| [platformio.ini](platformio.ini) | Selecciona placa, framework y version de plataforma; configura flash, particiones y monitor serie. |
| [CMakeLists.txt](CMakeLists.txt) | Inicia el sistema de compilacion de ESP-IDF. |
| [sdkconfig.defaults](sdkconfig.defaults) | Valores iniciales para memoria, consola, logs y otras opciones de ESP-IDF. |
| [partitions.csv](partitions.csv) | Distribuye la flash entre almacenamiento reservado, datos PHY y aplicacion. |
| [include/lab_config.h](include/lab_config.h) | Configuracion central de la aplicacion: nombre, version, consola, umbrales y reserva de pines. |
| [src/CMakeLists.txt](src/CMakeLists.txt) | Enumera los archivos de codigo y las dependencias que deben compilarse. |
| [src/main.c](src/main.c) | Punto de entrada `app_main`: configura logs, muestra el inicio, lanza diagnosticos y arranca la CLI. |
| [src/core/cli.h](src/core/cli.h) | Declara la funcion de arranque de la consola. |
| [src/core/cli.c](src/core/cli.c) | Crea la consola UART, registra el menu y los comandos de cada modulo, y comienza a atender entradas. |
| [src/core/module.h](src/core/module.h) | Define el contrato comun de un modulo: nombre, descripcion, comando, diagnostico y estado. |
| [src/core/module.c](src/core/module.c) | Mantiene el registro de modulos, genera el menu y coordina los diagnosticos. |
| [src/modules/system.c](src/modules/system.c) | Implementa `system info`, `system diag` y `system pins`. |
| [src/modules/wifi.c](src/modules/wifi.c) | Implementa el modulo Wi-Fi en modo estacion. |
| [src/modules/wifi_audit.c](src/modules/wifi_audit.c) | Analiza seguridad anunciada, referencias conocidas y PMF de beacons/respuestas de sondeo. |
| [src/modules/ble.c](src/modules/ble.c) | Escaneo y cliente GATT con NimBLE. |
| [src/modules/nfc.c](src/modules/nfc.c) | PN532 I2C, UID y paginas NTAG. |
| [src/modules/cc1101_radio.cpp](src/modules/cc1101_radio.cpp) | RadioLib y operaciones de paquetes CC1101. |
| [src/modules/ir.c](src/modules/ir.c) | Captura/emision RMT y senales NVS. |
| [src/modules/gpio.c](src/modules/gpio.c) | Entradas, salidas y PWM con pines restringidos. |
| [src/modules/rfid.c](src/modules/rfid.c) | Recepcion UART y validacion de tramas RDM6300. |
| [src/modules/ibutton.c](src/modules/ibutton.c) | Enumeracion de ROM 1-Wire. |
| [src/modules/storage.c](src/modules/storage.c) | microSD por SPI y operaciones FAT limitadas. |
| [src/modules/usb_hid.c](src/modules/usb_hid.c) | Teclado/raton con TinyUSB. |
| [src/core/spi_bus.c](src/core/spi_bus.c) | Inicializacion compartida de SPI2 para SD y CC1101. |
| [src/core/command_args.h](src/core/command_args.h) | Validacion comun de numeros, hex y nombres. |
| [src/idf_component.yml](src/idf_component.yml) | Dependencias externas del gestor de componentes. |
| [.gitignore](.gitignore) | Evita incluir entornos y archivos generados en el control de versiones Git. |

### Archivos y carpetas generados

- `.venv/`: herramientas Python locales del ordenador.
- `.pio/`: resultados de compilacion y otros archivos de trabajo de PlatformIO.
- `managed_components/`: dependencias descargadas; se excluyen de Git, conservando el manifiesto y el lock.
- [sdkconfig.esp32s3_n16r8](sdkconfig.esp32s3_n16r8): configuracion efectiva que ESP-IDF genera para este entorno. Se crea al configurar/compilar; no es el archivo de valores iniciales.

Los enlaces a archivos generados solo funcionaran despues de la primera compilacion. No necesitas editar el binario ni los archivos internos de `.pio`.

La particion de aplicacion actual ocupa 2 MiB. El porcentaje de ocupacion se calcula respecto a esa particion, **no respecto a los 16 MB completos de flash**. NVS guarda referencias y senales mediante comandos explicitos y puede llenarse; no se borra automaticamente para recuperar espacio. El resto de flash no se convierte automaticamente en un sistema de archivos. La microSD es almacenamiento externo independiente.

### Recorrido de un comando

Cuando escribes `system info`, el monitor envia el texto por USB al puente UART. La consola de ESP-IDF reconoce `system` y llama al manejador del modulo. El manejador interpreta `info`, consulta el sistema y escribe una respuesta por la misma conexion. Los demas modulos siguen ese contrato; las operaciones se solicitan desde la consola, no desde el menu ni los diagnosticos.

## 9. Configuracion y reserva de pines

### Donde cambiar cada cosa

Empieza por [include/lab_config.h](include/lab_config.h) si quieres cambiar el nombre del dispositivo, el texto `lab>`, la version, los limites de consola o la propuesta de pines. Los cambios en el codigo requieren **compilar y flashear de nuevo** para llegar a la placa.

Las opciones propias de ESP-IDF se encuentran en [sdkconfig.defaults](sdkconfig.defaults). PlatformIO y la velocidad del monitor se configuran en [platformio.ini](platformio.ini). Hay varios archivos porque pertenecen a niveles distintos: aplicacion, framework y herramientas de compilacion.

**Importante:** los valores de `sdkconfig.defaults` no sustituyen automaticamente los ya guardados en la configuracion efectiva generada. Para modificar la configuracion efectiva puedes utilizar, desde PowerShell:

```powershell
.\.venv\Scripts\pio.exe run -e esp32s3_n16r8 -t menuconfig
```

Guarda los cambios, sal del menu, compila y flashea otra vez. Si quieres reproducir esas opciones en una instalacion limpia, manten tambien actualizados los valores iniciales correspondientes. No cambies el modo de flash o PSRAM sin verificar antes la variante real de la placa.

La velocidad del firmware procede de la opcion UART de ESP-IDF. Si la cambias, ajusta tambien `monitor_speed` en la configuracion de PlatformIO. Para empezar, deja ambos a 115200.

**Diagnostico Wi-Fi pendiente:** se conserva flash QIO de 16 MB a 40 MHz y PSRAM octal a 80 MHz. La reduccion de flash de 80 a 40 MHz se preparo para investigar reinicios; no es una reparacion confirmada. La ampliacion actual tambien cambia el codigo, dependencias y tick FreeRTOS a 1000 Hz, por lo que ya no es una comparacion aislada de frecuencia. No cambies ajustes de memoria sin un ensayo separado y una imagen identificada.

### Cableado previsto

**Verifica cada modelo antes de conectarlo.** Los numeros son identificadores GPIO, no posiciones fisicas consecutivas de los conectores. Este cableado todavia no se ha probado con los perifericos reales.

| Funcion | GPIO reservados |
|---|---|
| SPI: reloj SCK / datos MOSI / datos MISO | 12 / 11 / 13 |
| CC1101: seleccion CS / senales GDO0 / GDO2 | 10 / 14 / 21 |
| I2C: datos SDA / reloj SCL | 8 / 9 |
| PN532: IRQ reservado pero no utilizado / reset | 4 / 5 |
| IR: emisor TX / receptor RX | 17 / 18 |
| microSD: CS, comparte SPI con CC1101 | 1 |
| RDM6300: TX del lector hacia RX UART1 del ESP32 | 2 |
| iButton: datos 1-Wire | 6 |
| GPIO de usuario | 7 / 15 / 16 / 47 |

SPI e I2C comunican chips mediante varias senales. SD y CC1101 comparten SPI2 con CS independientes, inicialmente altos; desmontar SD no libera el bus compartido. PN532 debe estar configurado fisicamente en I2C: este controlador sondea su estado y no conecta IRQ, para evitar interferencias entre servicios de interrupcion de dependencias.

- PN532: comprueba las resistencias pull-up I2C y que las lineas no suban a 5 V.
- CC1101: modulo y antena adecuados a la banda; senales a 3,3 V.
- microSD: interfaz SPI compatible con 3,3 V, alimentacion estable y pull-ups segun el adaptador. Evita adaptadores que retengan MISO cuando su CS esta inactivo.
- RDM6300: 9600 baudios, 8N1. Muchos lectores requieren alimentacion de 5 V; comprueba el nivel de TX y adapta a 3,3 V antes de GPIO2. La antena de 125 kHz corresponde al lector, no al ESP32.
- iButton: resistencia externa de aproximadamente 4,7 kohm entre datos y 3,3 V, con masa comun.
- IR RX: receptor demodulado activo-bajo de nivel compatible. IR TX: transistor o controlador, resistencia limitadora y alimentacion dimensionada; nunca LED de potencia directo al GPIO.
- USB HID: utiliza el conector nativo de la placa, GPIO19/20, sin reutilizarlos como GPIO. Revisa la alimentacion antes de conectar simultaneamente USB-UART y USB nativo.

Pines que este proyecto mantiene fuera de esas asignaciones:

| Grupo | Motivo |
|---|---|
| GPIO43 / GPIO44 | UART0 utilizada por la consola. |
| GPIO19 / GPIO20 | USB nativo reservado. |
| GPIO26 a GPIO37 | Grupo excluido por las conexiones y restricciones de memoria del modulo; en la variante octal no debes reutilizar GPIO35, GPIO36 ni GPIO37. |
| GPIO0 / GPIO3 / GPIO45 / GPIO46 | Pines relacionados con la configuracion de arranque. |
| GPIO38 / GPIO48 | LED RGB segun la revision de la placa; se reservan ambos por precaucion. |
| GPIO39 a GPIO42 | Reservados para posible depuracion JTAG externa. |

La serigrafia y la documentacion del fabricante son necesarias para decidir el cableado final. Tener un controlador compilado no permite asumir que cualquier modulo con un nombre parecido sea compatible.

## 10. Problemas habituales

| Sintoma | Que comprobar |
|---|---|
| `python` no se reconoce o abre Microsoft Store | Revisa la instalacion de Python y su PATH. Abre una terminal nueva despues de instalarlo. |
| No existe `.\.venv\Scripts\pio.exe` | Confirma que estas en la carpeta correcta y completa la creacion de `.venv` e instalacion de PlatformIO. |
| Falla la descarga de herramientas | Comprueba Internet, proxy, espacio libre y el mensaje exacto del error. No sigas al flasheo con una compilacion fallida. |
| No aparece ningun puerto COM | Usa el conector USB-to-UART y otro cable de datos. Revisa el Administrador de dispositivos. Si falta el controlador, instala el del fabricante del puente USB-UART de tu placa. |
| Hay varios puertos COM | Compara la lista antes y despues de conectar la placa. No flashees un puerto que no hayas identificado. |
| Puerto ocupado o acceso denegado | Cierra monitores serie y cualquier otra aplicacion que este usando ese COM. |
| El flasheo se queda en `Connecting` | Comprueba el puerto y utiliza la secuencia BOOT/RESET explicada en la seccion 5. |
| El monitor muestra caracteres ilegibles | Confirma 115200 baudios en firmware y monitor, y que estas usando el COM correcto. |
| No aparece `lab>` | Pulsa RESET con el monitor abierto. Comprueba que no estas en USB nativo y busca errores anteriores en los logs de arranque. |
| Solo ves mensajes de modo descarga | Pulsa RESET sin mantener BOOT despues del flasheo. |
| `ERR USAGE` | Revisa los argumentos del comando; por ejemplo, usa `system info` en vez de `system`. |
| `SKIP` | El diagnostico no ha probado ese hardware. No conectes un modulo solo para intentar eliminar el mensaje. |
| Error NVS por falta de espacio/version | Conserva los datos necesarios y revisa la particion; no hay borrado automatico. Los comandos `ir remove` y `wifi trust erase` eliminan solo datos solicitados. |
| `sd write` no crea un archivo | Comprueba montaje, espacio, nombre permitido y que no exista; no sobrescribe ni formatea. |
| Tamano de PSRAM o flash incorrecto | Verifica que la placa sea realmente N16R8 y revisa la configuracion efectiva y los logs. No cambies solo el valor esperado para ocultar el problema. |
| Reinicios, brownout o desconexiones | Deja solo la placa conectada, prueba otro cable/puerto USB y comprueba alimentacion y logs. |
| `wifi scan` reinicia tras `ic_enable_sniffer` | El escaneo actual no activa el modo promiscuo; compila, flashea y comprueba la identidad de la imagen. La captura solo se solicita con `wifi pmf <BSSID>`. Conserva el log completo si vuelve a fallar. |
| `wifi scan` reinicia con `LoadProhibited` sin `ic_enable_sniffer` | El registro `cbc2303f4` falla dentro del analizador RSN del SDK. Se ha preparado una prueba con flash a 40 MHz; no esta confirmada la causa. Prueba primero solo el escaneo normal y conserva el log completo desde RESET. |
| `wifi pmf` reinicia o falla al limpiar la radio | El comando es experimental: la causa del reinicio anterior sigue pendiente. Conserva el log desde RESET y deja de usar PMF hasta depurarlo. Ante un error de limpieza, reinicia la placa antes de usar Wi-Fi. No desactives el watchdog. |
| Cambiaste el codigo pero no cambia la placa | Compilar no actualiza la placa: tambien debes flashear el nuevo firmware. |

Para pedir ayuda, incluye el comando que ejecutaste, el primer error completo, la variante de placa, el conector utilizado y los logs desde RESET. No te limites a la ultima linea `FAILED`.

## 11. Como continuar el desarrollo

La arquitectura separa la comunicacion de la logica de cada modulo. Esto permite desarrollar un periferico sin rehacer la consola.

Los modulos anteriores ya tienen manejadores y controladores, pero sus limites son los descritos en la seccion 6. No se ha implementado toda la funcionalidad de Flipper Zero: quedan, entre otras, emulacion/escritura LF e iButton, emulacion y formatos NFC adicionales, protocolos de mandos sub-GHz, decodificacion/importacion IR, notificaciones/emparejamiento BLE e integracion de archivos entre modulos. No hay interfaz grafica ni aplicaciones Flipper compatibles.

Para anadir un modulo completamente nuevo habra que definir su descriptor, declararlo en la cabecera comun, incorporarlo al registro y agregar su codigo a la lista de compilacion. La consola y el menu utilizaran ese registro.

Antes de conectar hardware, cierra la prueba basica: arranque, comunicacion estable y diagnosticos de memoria. Despues verifica **un solo periferico** y su cableado de forma independiente, empezando por lecturas. Solo prueba escrituras y emisiones con elementos propios prescindibles y confirmacion explicita.

## 12. Estado de las pruebas

Herramientas utilizadas para compilar y verificar la ampliacion:

| Herramienta | Version |
|---|---|
| Python del entorno local | 3.10.11 |
| PlatformIO Core | 6.1.18 |
| Plataforma Espressif32 de PlatformIO | 6.10.0 |
| ESP-IDF | 5.4.0 |

Resultado de compilacion de la ampliacion: **SUCCESS**, con 1.214.952 bytes de programa (57,9 % de la particion de 2 MiB) y 54.708 bytes de RAM estatica (16,7 % de 320 KiB). La RAM estatica no incluye las asignaciones dinamicas de radios, USB, filesystem y controladores; estos valores cambian con el codigo y la configuracion.

Se verifico que la configuracion generada selecciona flash de 16 MB, PSRAM octal a 80 MHz y consola UART0 a 115200 baudios. Los archivos de la aplicacion se compilan con avisos estrictos y los avisos tratados como errores.

**Pendiente:** flasheo y prueba real de esta ampliacion. No se ha flasheado ni abierto el puerto serie durante su implementacion. El usuario si aporto registros de Wi-Fi de versiones anteriores, descritos abajo. Compilar no demuestra que los perifericos funcionen; los ejemplos no son capturas de pruebas fisicas.

### Comprobacion de perifericos pendiente

1. Arrancar sin modulos: comprobar consola, memoria, menu y ausencia de activacion de buses/salidas/HID por los diagnosticos.
2. Conectar cada lector por separado y probar ausencia de tarjeta, tarjeta valida y retirada durante lectura. Un timeout debe devolver el control sin reiniciar.
3. PN532: comprobar UID y paginas permitidas; rechazar paginas protegidas; probar escritura y lectura posterior solo en una NTAG propia prescindible.
4. IR: aprender una senal conocida, mostrarla, guardar/cargar tras RESET y comprobar una unica emision con receptor de prueba. Contrastar portadora y duraciones con instrumentacion.
5. microSD: crear un archivo de prueba; repetir el nombre y comprobar que no cambia; leer, desmontar y verificarlo en el PC. Probar errores con una tarjeta prescindible, nunca una con datos importantes.
6. CC1101: recibir de un equipo compatible; despues transmitir una vez en una configuracion legal. Comprobar ausencia de modulo, plazo RX, cooldown y uso compartido con SD.
7. GPIO: medir los cuatro pines permitidos, PWM y liberacion; comprobar rechazo de pines reservados sin alterar consola ni memoria.
8. BLE: probar GATT propio con permisos de lectura/escritura conocidos, desconexion remota y timeout; comprobar que ninguna operacion usa una conexion antigua.
9. USB: probar ASCII en una ventana vacia propia, liberacion de teclas, retirada del cable y stop/start. Comprobar que nunca envia entradas al arrancar sin orden explicita.
10. Verificar persistencia, NVS llena, errores de bus y ciclos repetidos de comandos. Las pruebas locales no cubren estas rutas de driver ni sus temporizaciones.

### Pruebas locales y Wi-Fi

Las pruebas locales de [tests/run_wifi_audit.py](tests/run_wifi_audit.py) compilan la logica real de auditoria sin radio ni acceso a puertos COM:

```powershell
.\.venv\Scripts\python.exe tests/run_wifi_audit.py
```

Requieren Zig, Clang o GCC; consulta `--help` para preparar el compilador. Se verificaron 6.325 comprobaciones de auditoria y 2.317 de argumentos en cada modo `signed-char` y `unsigned-char`, con avisos como errores y comprobaciones de comportamiento indefinido. Incluyen PMF opcional/obligatorio/no soportado, anuncios truncados o fragmentados, longitudes y contadores invalidos, elementos duplicados, listas multiples de cifrado/AKM y campos RSN opcionales. El filtro individual prueba BSSID/origen, tramas cortas, cabeceras invalidas y multicast. [tests/test_command_args.c](tests/test_command_args.c) prueba numeros, desbordamientos, hex, capacidad de buffers y nombres que intentan incluir rutas. No simulan los controladores fisicos.

Las pruebas en placa aportadas por el usuario mostraron reinicios al activar la captura PMF (`ic_enable_sniffer`), incluyendo errores de cache, excepciones y watchdog. Mover el callback y sus auxiliares a IRAM no resolvio el fallo: el registro posterior con identificador de aplicacion `4488c0421` tambien se reinicio. La causa exacta sigue pendiente de depuracion.

En una mitigacion anterior se deshabilito toda la captura PMF. Ahora se ha retirado del escaneo y solo se activa explicitamente con `wifi pmf <BSSID>`. La compilacion del comando individual se verifico con ESP-IDF; las pruebas locales no ejecutan el driver ni prueban sus errores, la restauracion de la radio o su estabilidad. El usuario probo esta variante en placa y aporto un nuevo reinicio durante el escaneo normal; la consulta PMF individual sigue sin validar.

El nuevo registro corresponde a la imagen `cbc2303f4`, cuyo identificador incorporado se cotejo con el binario local antes de resolver la traza. La cadena incluye `scan_parse_beacon`, `wpa_parse_wpa_ie_wrapper`, `wpa_parse_wpa_ie_rsn` y `rsn_selector_to_bitfield` del SDK. No aparece nuestro analizador PMF ni se activa `ic_enable_sniffer`. El PC `0x42043a8c` y `EXCVADDR=0x82043d5e` no bastan para atribuir la causa a una red, al hardware o al SDK.

Historicamente se preparo una prueba aislada de flash a 40 MHz, con identificador `bc9fbfc40`; no se recibio su resultado en placa. **Ese identificador no corresponde a la ampliacion actual**, que cambia tambien codigo, dependencias y tick FreeRTOS. Se conserva flash a 40 MHz, sin dar el fallo por resuelto. Tras flashear la imagen actual, incluido el bootloader, verificar `SPI Speed : 40MHz` y su nuevo identificador. Ejecutar cinco veces `wifi scan`, esperando entre comandos, sin PMF. Si reinicia, detener la prueba y conservar el log completo; si pasa, repetir tras RESET. Un resultado estable no demuestra la causa original.

Para identificar una imagen utiliza el campo `ELF file SHA256` de la informacion de aplicacion incorporada al binario, que es el que imprime la placa. El hash obtenido con `Get-FileHash firmware.elf` puede diferir porque el proceso de generacion modifica el ELF. En este entorno puede consultarse asi:

```powershell
.\.venv\Scripts\python.exe "$env:USERPROFILE\.platformio\packages\tool-esptoolpy\esptool.py" --chip esp32s3 image_info --version 2 .pio/build/esp32s3_n16r8/firmware.bin
```

**Pendiente en hardware:** cerrar el monitor, flashear siguiendo la seccion 5 y ejecutar varias veces `wifi scan` y `wifi audit`, con el ESP32 conectado y desconectado. Deben listar resultados y volver a `lab>` sin activar `ic_enable_sniffer` ni mostrar campos PMF. Comprobar tambien `wifi suspects` con referencias registradas.

Para la consulta individual, comprobar primero que rechaza una direccion invalida, un BSSID ausente, un escaneo de mas de 120 segundos y una estacion conectada. Desconectar, escanear y ejecutar `wifi pmf <BSSID>` sobre un AP propio: debe devolver un estado anunciado o `unknown` al agotar el plazo, y permitir un nuevo escaneo y conexion despues. Contrastar los estados con la configuracion PMF del AP. Si se reinicia al activar la captura, conservar el log completo y no dar por resuelto el fallo original. Los errores de inicio/parada y restauracion requieren pruebas adicionales del driver o inyeccion de fallos.

Documentacion de referencia: [guia oficial de ESP32-S3 DevKitC-1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html).