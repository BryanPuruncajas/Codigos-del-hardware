# Pruebas N03 y preparación de integración con ESP

Actualizado el 9 de octubre de 2026. Banco manual, motores desconectados.
No valida vuelo autónomo ni contacto. Las identidades correctas fueron
confirmadas por el operador; los logs no reconocen la identidad física.

## Configuración vigente

- N03-panel-v2-central: adquisición de un único F con cx entre 90 y 150,
  inclusive, sobre imagen de 240 × 160. Dos F dentro: AMBIG; ninguno: SEARCH.
- Confirmación: 5 imágenes y 300 ms. La franja no limita el seguimiento
  después de adquirir. No representa una tolerancia angular calibrada.
- LOST después de más de 2000 ms sin asociación, bloqueado hasta reiniciar.
- Llegada candidata: caja observada >=180 × 150 durante 2000 ms continuos.
  No significa contacto ni distancia medida. Los 44 cm son referencia manual.
- Una pulsación de 1 acepta la llegada candidata y entra en GIRO; otra
  habilita buscar el siguiente. El giro todavía lo confirma el operador.
- 22 pruebas locales aprobadas; USB y JPEG probados en Nicla. Sin integración
  del panel con ESP ni órdenes a actuadores. N02 original conservado.

## Tabla de ensayos

Todos los archivos están en `n03_panel/logs/`. Tiempos desde la primera muestra.

| Registro JSONL | Versión | Ensayo | Resultado | Alcance |
| --- | --- | --- | --- | --- |
| 20261009_171444_307913 | N03 inicial | Secuencia continua | 5 adquisiciones, 5 llegadas aceptadas, 4 habilitaciones del siguiente | Solo un globo visible a la vez; no prueba competencia entre candidatos |
| 20261009_172010_012106 | N03 inicial | Varios candidatos | 4 adquisiciones, 3 llegadas aceptadas; T4 termina en LOST | No prueba elección entre dos F en cada adquisición |
| 20261009_172612_047789 | N03 inicial | Selección por área | Adquirió el izquierdo a 13.935 s tras ambigüedad y confirmación | Motivó selección central por sensibilidad al encuadre |
| 20261009_173631_439790 | N03 central | Adquisición central | Adquirió a 30.42 s; unos 9 s sin perderlo; 53 muestras con dos F | Operador confirmó objetivo correcto |
| 20261009_180602_711669 | N03 central | Aproximación 1 | Adquirió 10.32 s; aviso 25.37 s; giro 28.14 s; 2 MISSING recuperados; sin LOST | Objetivo correcto y sin cambio, confirmado por operador |
| 20261009_180701_047817 | N03 central | Aproximación 2 | Adquirió 13.68 s; aviso 32.36 s; giro 35.51 s; 3 MISSING recuperados; sin LOST | Objetivo correcto y sin cambio, confirmado por operador |
| 20261009_180758_125490 | N03 central | Aproximación 3 | Adquirió 4.64 s; aviso 19.51 s; giro 21.77 s; sin MISSING ni LOST | Objetivo correcto y sin cambio, confirmado por operador |

Resultado: 3/3 elecciones finales correctas y conservadas según el operador.
No es una precisión general estimada. La secuencia de cinco objetivos es
anterior al cambio central; no se presenta como un único ensayo con v2.
No se guardó video sincronizado. Los tiempos de transición son muestreados.

## Siguiente fase de integración sin motores

La ESP es el cerebro de control. La Nicla entrega observaciones; no gobierna
la misión ni autoriza motores. El contrato siguiente es propuesto, aún no implementado.

| Dirección | Datos o decisión |
| --- | --- |
| Nicla a ESP | Secuencia y tiempo, estado, observed, ID local, centro, caja y recorte |
| ESP a Nicla | Habilitar adquisición, seguir o suspender durante giro; orden identificada y confirmada |
| ESP a actuadores | Centrado, avance, frenado y giro según misión, sensores y seguridad |

1. Revisar receptor y campos existentes en ESP; no confundir ROI histórico
   con centro y tamaño observados. Definir transporte y vigencia de medidas.
2. Probar sin motores izquierda/centro/derecha, aproximación, pérdida,
   desconexión, datos viejos y comandos duplicados.
3. Simular cambio de etapa desde ESP y confirmar que no adquiere durante giro.
4. Luego sustituir la segunda pulsación por un criterio medido de rumbo y
   estabilidad en ESP. No fijar aún tolerancias de yaw sin ensayo.
5. Validar frenado y contacto por separado antes de navegación autónoma.

La figura de competencia aportada indica 4 m × 2 m entre esquinas dentro de
10 m × 5 m. Course 1: 1 → 2 → 3 → 4 → 1. Course 2: 1 → 3 → 4 → 2 → 1.
El requisito es visitar y tocar; el aviso visual no lo acredita.
