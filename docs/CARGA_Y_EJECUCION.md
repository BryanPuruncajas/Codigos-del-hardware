# Carga del codigo y ejecucion del sistema

Guia para Windows, usando **CMD**. Todas las rutas son relativas a la raiz de `Codigos-del-hardware`.

## 1. Descargar el proyecto

Solo para una computadora que todavia no tenga el repositorio:

```bat
git clone --branch competencia-v42-flowdeck https://github.com/BryanPuruncajas/Codigos-del-hardware.git
cd Codigos-del-hardware
```

Si ya existe una copia, abrir CMD en la raiz de esa copia. No es necesario volver a clonar.

## 2. Preparar las herramientas

Instalar Python, Git, Flutter (con Android SDK) y OpenMV IDE. Luego, desde la raiz:

```bat
python -m pip install platformio
python -m pip install -r groundstation\requirements.txt
python -m platformio device list
```

En los comandos siguientes, sustituir `COM_ESP`, `COM_GROUND` y `COM_FLOW` por los puertos reales que muestra el sistema. No son nombres de puertos literales.

## 3. Cargar la ESP principal del blimp

Por el circuito de estas PCBs: retirar la ESP de la PCB, desconectar su bateria y alimentarla solo por USB durante la carga. No conectar USB y alimentacion de la PCB simultaneamente.

```bat
python -m platformio run -d firmware -t upload --upload-port COM_ESP
```

El comando compila y carga el firmware completo, incluyendo librerias propias, pruebas P y Balloon Mission. No se cargan esos archivos uno por uno. Esperar `[SUCCESS]`.

Desconectar el USB antes de volver a montar la ESP en la PCB y alimentar por bateria.

## 4. Cargar Ground

Identificar el puerto de la ESP Ground, sin confundirlo con el de la ESP principal. Ground permanece conectada por USB a la computadora durante el funcionamiento.

```bat
python -m platformio run -d base_station -t upload --upload-port COM_GROUND
```

Cerrar el puente de telemetria o cualquier monitor que tenga abierto ese puerto antes de cargar.

## 5. Cargar la ESP emisora de Flowdeck

Desconectar su bateria y cualquier alimentacion externa de esa placa; usar solo USB para la carga, fuera de una PCB alimentada.

```bat
python -m platformio run -d flowdeck_sender -t upload --upload-port COM_FLOW
```

Esto carga la ESP emisora UART/ESP-NOW. **No carga el firmware de la Crazyflie** que lee fisicamente Flowdeck; ese firmware se mantiene aparte y su fuente instalada no esta confirmada dentro de este repositorio. `firmware/flow_node` es un emisor de datos simulados para pruebas, no el emisor real de Flowdeck.

## 6. Cargar Nicla Vision

1. Retirar Nicla de la PCB y desconectar la alimentacion externa; conectarla solo por USB.
2. Abrir OpenMV IDE y conectar la placa.
3. Abrir `vision/n03_panel/nicla_balloon_mission_main.py`.
4. Usar **Herramientas > Guardar script abierto como main.py**.
5. Desconectar USB antes de montar Nicla en la PCB y alimentar externamente. Al reiniciar, ejecutara el `main.py` guardado.

## 7. Iniciar Ground y telemetria

Montar las placas sin sus USB y alimentar el blimp por bateria. Conectar solo Ground por USB a la computadora. Mantener cerrados otros programas que intenten abrir el puerto de Ground.

En una primera ventana CMD, desde la raiz:

```bat
python groundstation\telemetry_bridge.py COM_GROUND
```

Mantener esa ventana abierta. Si es necesario conocer la IPv4 de la laptop:

```bat
ipconfig
```

## 8. Instalar y ejecutar la app

Conectar el celular por USB, activar depuracion USB y aceptar su autorizacion. En otra ventana CMD, desde la raiz:

```bat
cd blimp_app
flutter pub get
flutter devices
flutter run
```

Si Flutter ofrece varios dispositivos, seleccionar el celular. Conectar celular y laptop a la misma red Wi-Fi. En la app, ingresar la IPv4 de la laptop; Ground debe seguir ejecutandose.

Instalar el firmware o abrir Ground/app no inicia por si solo una prueba de vuelo. Las pruebas se activan mediante los controles de la app o sus lanzadores especificos.

## 9. Actualizar esta guia en GitHub

Desde la raiz, verificar que se este en `competencia-v42-flowdeck` y ejecutar personalmente:

```bat
git branch --show-current
git add -- docs/CARGA_Y_EJECUCION.md
git commit -m "Agrega guia de carga y ejecucion del sistema"
git push origin competencia-v42-flowdeck
```
