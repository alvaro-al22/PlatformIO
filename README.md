# USB Lab: laboratorio educativo con ESP32-S3

Este proyecto convierte un ESP32-S3 en la base de un laboratorio modular controlado desde el ordenador. No necesitas pantalla, botones externos ni bateria: un cable USB proporciona alimentacion y comunicacion.

Estamos en la **fase 1: infraestructura**. Ya existe un firmware con consola, menu y diagnostico del sistema. Los modulos NFC, radio, infrarrojos y GPIO tienen su espacio preparado, pero todavia no realizan operaciones con el hardware.

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
| ESP32-S3 | El microcontrolador: el chip que ejecuta el programa y, en el futuro, controlara los perifericos. |
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
| Driver | Codigo que sabe comunicarse con un componente concreto. Los drivers externos aun no estan implementados. |
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
| NFC / PN532 | Marcador pendiente: responde `not implemented`. |
| CC1101 | Marcador pendiente: no transmite ni recibe radio. |
| IR | Marcador pendiente: no emite ni recibe infrarrojos. |
| GPIO | Marcador pendiente: no permite leer ni modificar pines. |

Wi-Fi y Bluetooth no se inicializan. No hay pantalla, interfaz grafica, bateria, PCB propia ni carcasa.

La CLI **se ejecuta en el ESP32**. El ordenador solo actua como terminal. El menu es una lista de comandos: no se navega con botones ni seleccionando numeros.

## 3. Material y conexiones

### Lo necesario ahora

- ESP32-S3 DevKitC-1 N16R8.
- Cable USB compatible con la placa y capaz de transferir datos.
- Ordenador con Windows y acceso a Internet para la instalacion inicial.

La variante N16R8 tiene 16 MB de flash y 8 MB de PSRAM octal. El ESP32-S3 tiene dos nucleos y puede alcanzar 240 MHz; eso no significa que este proyecto lo configure necesariamente a esa frecuencia maxima.

### Lo que debes dejar desconectado

Por ahora no conectes PN532, CC1101, emisor IR, receptor IR ni cables a GPIO. No hacen falta para probar esta fase. Tampoco se utilizan componentes duplicados.

### Elegir el conector USB correcto

La DevKitC-1 dispone de un puerto **USB-to-UART** y otro conectado al **USB nativo del ESP32-S3**. Identificalos por las etiquetas de tu placa o por su documentacion, no por su posicion: las variantes pueden diferir.

**Este firmware usa el puerto USB-to-UART.** El USB nativo no esta configurado como su consola interactiva.

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
   system / nfc / cc1101 / ir / gpio
```

La comunicacion usa **115200 baudios, 8 bits de datos, sin paridad, 1 bit de parada y sin control de flujo**: suele abreviarse como 115200, 8N1. Estos ajustes ya estan preparados en el proyecto.

**Seguridad electrica:** la placa se alimenta por USB, pero sus GPIO trabajan a 3,3 V. No apliques 5 V a un GPIO ni alimentes modulos desconocidos sin comprobar sus especificaciones. No conectes otra fuente de alimentacion en esta fase. Desconecta el USB antes de cambiar cableado.

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
Set-Location "C:\Users\alvaro.arnaiz\Desktop\TCH\cloud\pruebas"
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

| Comando | Que hace |
|---|---|
| `help` | Muestra la ayuda de los comandos registrados en la consola. |
| `menu` | Lista los cinco modulos y su estado de implementacion. |
| `system info` | Muestra version de firmware y ESP-IDF, chip, tiempo encendido, motivo de reinicio y heap libre. |
| `system diag` | Repite el diagnostico del sistema y muestra los modulos que no se han comprobado. |
| `system pins` | Muestra las reservas de GPIO. No lee, activa ni modifica esos pines. |
| `nfc` | Devuelve `not implemented`. No detecta ni lee tarjetas. |
| `cc1101` | Devuelve `not implemented`. No realiza operaciones de radio. |
| `ir` | Devuelve `not implemented`. No controla los modulos IR. |
| `gpio` | Devuelve `not implemented`. No cambia niveles electricos. |

Prueba inicial recomendada, ejecutando una linea cada vez dentro de la consola:

```text
help
menu
system info
system diag
system pins
nfc
cc1101
ir
gpio
```

Por ejemplo, al ejecutar `nfc` es normal recibir:

```text
ERR NOT_IMPLEMENTED nfc: not implemented; hardware not probed
```

No indica que el PN532 este roto: significa que aun no existe su implementacion. La consola tambien puede mostrar el codigo de error devuelto por el comando.

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
| `ready` en el menu | El modulo tiene implementacion; no es el resultado de una prueba electrica. |
| `I (...)` | Log informativo. |
| `W (...)` | Advertencia que conviene revisar. |
| `E (...)` | Log de error. |

Es normal ver cuatro lineas `SKIP`, una por cada modulo pendiente. Si las comprobaciones implementadas pasan, el resumen sera:

```text
OK implemented checks passed; SKIP is not PASS
```

### Lo que no puede diagnosticar todavia

No detecta si has conectado un PN532, CC1101 o modulo IR. No comprueba antenas, cableado, cortocircuitos, tensiones de GPIO ni calidad de una senal. Comprobar reservas de pines solo valida la configuracion del programa.

La comprobacion de flash verifica su tamano, no todos sus sectores. La integridad del heap comprueba su gestion interna, no es un ensayo exhaustivo de toda la RAM. ESP-IDF tiene ademas habilitada su prueba de PSRAM durante el arranque.

Si el diagnostico de la aplicacion falla, esta intenta mantener disponible la consola. Sin embargo, un fallo anterior, durante el arranque de ESP-IDF, puede impedir llegar a ella. La configuracion permite continuar si la PSRAM no se encuentra cuando ESP-IDF puede recuperarse de ese caso, pero no garantiza recuperacion de cualquier fallo de memoria.

Respuestas y logs comparten la misma conexion. Es una consola para personas, **no un protocolo JSON ni una API automatizada** con identificadores de peticion. Los logs no se guardan automaticamente en un archivo del ordenador ni en la flash.

## 8. Como esta organizado el proyecto

Los archivos `.c` contienen implementaciones en C; los `.h` contienen declaraciones y configuracion que pueden compartir varios archivos.

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
| [src/modules/nfc.c](src/modules/nfc.c) | Declara el modulo PN532 pendiente. |
| [src/modules/cc1101.c](src/modules/cc1101.c) | Declara el modulo de radio pendiente. |
| [src/modules/ir.c](src/modules/ir.c) | Declara el modulo de infrarrojos pendiente. |
| [src/modules/gpio.c](src/modules/gpio.c) | Declara el modulo de GPIO pendiente. |
| [.gitignore](.gitignore) | Evita incluir entornos y archivos generados en el control de versiones Git. |

### Archivos y carpetas generados

- `.venv/`: herramientas Python locales del ordenador.
- `.pio/`: resultados de compilacion y otros archivos de trabajo de PlatformIO.
- [sdkconfig.esp32s3_n16r8](sdkconfig.esp32s3_n16r8): configuracion efectiva que ESP-IDF genera para este entorno. Se crea al configurar/compilar; no es el archivo de valores iniciales.

Los enlaces a archivos generados solo funcionaran despues de la primera compilacion. No necesitas editar el binario ni los archivos internos de `.pio`.

La particion de aplicacion actual ocupa 2 MiB. El porcentaje de ocupacion que muestra PlatformIO se calcula respecto a esa particion, **no respecto a los 16 MB completos de flash**. El resto no se convierte automaticamente en un sistema de archivos. Las particiones NVS y PHY estan reservadas; esta fase no implementa guardado de preferencias ni activa la radio por tener esas particiones.

### Recorrido de un comando

Cuando escribes `system info`, el monitor envia el texto por USB al puente UART. La consola de ESP-IDF reconoce `system` y llama al manejador del modulo. El manejador interpreta `info`, consulta el sistema y escribe una respuesta que regresa por la misma conexion. Los demas modulos siguen ese mismo contrato, aunque ahora solo devuelven el mensaje de pendiente.

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

### Propuesta para fases futuras

**Esta tabla es una reserva logica, no una instruccion para conectar los modulos ahora.** Los numeros son identificadores GPIO, no posiciones fisicas consecutivas de los conectores.

| Funcion futura | GPIO reservados |
|---|---|
| SPI: reloj SCK / datos MOSI / datos MISO | 12 / 11 / 13 |
| CC1101: seleccion CS / senales GDO0 / GDO2 | 10 / 14 / 21 |
| I2C: datos SDA / reloj SCL | 8 / 9 |
| PN532: interrupcion IRQ / reset, si el modulo los expone | 4 / 5 |
| IR: emisor TX / receptor RX | 17 / 18 |
| Expansion general | 1 / 2 / 6 / 7 / 15 / 16 / 47 |

SPI e I2C son formas de comunicar chips mediante varias senales. IRQ permite que un periferico avise al microcontrolador. Reservar estos pines evita que dos futuros modulos intenten usar el mismo recurso por accidente, pero no inicializa ninguno de esos buses.

Pines que este proyecto mantiene fuera de esas asignaciones:

| Grupo | Motivo |
|---|---|
| GPIO43 / GPIO44 | UART0 utilizada por la consola. |
| GPIO19 / GPIO20 | USB nativo reservado. |
| GPIO26 a GPIO37 | Grupo excluido por las conexiones y restricciones de memoria del modulo; en la variante octal no debes reutilizar GPIO35, GPIO36 ni GPIO37. |
| GPIO0 / GPIO3 / GPIO45 / GPIO46 | Pines relacionados con la configuracion de arranque. |
| GPIO38 / GPIO48 | LED RGB segun la revision de la placa; se reservan ambos por precaucion. |
| GPIO39 a GPIO42 | Reservados para posible depuracion JTAG externa. |

Antes de implementar cualquier periferico habra que comprobar su modelo exacto, alimentacion, niveles logicos y modo de interfaz. La serigrafia del modulo y la documentacion del fabricante son necesarias para decidir el cableado final.

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
| `not implemented` o `SKIP` | Es el comportamiento esperado de los modulos pendientes. No necesitas conectarlos para eliminar esos mensajes. |
| Tamano de PSRAM o flash incorrecto | Verifica que la placa sea realmente N16R8 y revisa la configuracion efectiva y los logs. No cambies solo el valor esperado para ocultar el problema. |
| Reinicios, brownout o desconexiones | Deja solo la placa conectada, prueba otro cable/puerto USB y comprueba alimentacion y logs. |
| Cambiaste el codigo pero no cambia la placa | Compilar no actualiza la placa: tambien debes flashear el nuevo firmware. |

Para pedir ayuda, incluye el comando que ejecutaste, el primer error completo, la variante de placa, el conector utilizado y los logs desde RESET. No te limites a la ultima linea `FAILED`.

## 11. Como continuar el desarrollo

La arquitectura separa la comunicacion de la logica de cada modulo. Esto permite desarrollar un periferico sin rehacer la consola.

Para completar un modulo existente, el trabajo futuro sera implementar su manejador de comandos y, si procede, su funcion de diagnostico; despues se actualizara su estado. Los cuatro modulos pendientes ya estan registrados y sus archivos ya forman parte de la compilacion. Marcar un modulo como implementado no crea por si solo un driver.

Para anadir un modulo completamente nuevo habra que definir su descriptor, declararlo en la cabecera comun, incorporarlo al registro y agregar su codigo a la lista de compilacion. La consola y el menu utilizaran ese registro.

Antes de conectar hardware, conviene cerrar la prueba basica: arranque correcto, comunicacion estable, comandos de sistema y diagnosticos de memoria. El siguiente paso sera escoger **un solo periferico** y verificar su cableado y driver de forma independiente. Esa implementacion no forma parte de esta fase.

## 12. Estado de las pruebas

La compilacion inicial se verifico el 29 de septiembre de 2026 con:

| Herramienta | Version |
|---|---|
| Python del entorno local | 3.10.11 |
| PlatformIO Core | 6.1.18 |
| Plataforma Espressif32 de PlatformIO | 6.10.0 |
| ESP-IDF | 5.4.0 |

Resultado: **SUCCESS**, con 298.196 bytes de programa y 14.344 bytes de RAM estatica reportados por la compilacion. Estos numeros pueden cambiar al modificar el codigo y no representan todo el uso de memoria durante la ejecucion.

Se verifico que la configuracion generada selecciona flash de 16 MB, PSRAM octal a 80 MHz y consola UART0 a 115200 baudios. Los archivos de la aplicacion se compilan con avisos estrictos y los avisos tratados como errores.

**Pendiente:** flasheo y prueba real en la placa. En la comprobacion inicial no se detectaron puertos serie. Compilar correctamente no demuestra que el cable, la placa o las memorias fisicas funcionen. Esta guia describe como realizar esa verificacion; los ejemplos de consola no son una captura de una prueba fisica ya realizada.

Documentacion de referencia: [guia oficial de ESP32-S3 DevKitC-1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.1.html).