# Secuencia de pruebas

El firmware nuevo contiene todos los modos; no se reflashea entre P01–P11. La base solo retransmite.

1. **P01** integración de sensores, sin actuadores.
2. **P02** control manual limitado. Verifica signo Fx/Fz/Tz.
3. **P03** lazo de yaw. Aquí se valida también la convención roll/pitch heredada antes de corregirla.
4. **P04** lazo de altura.
5. **P05** yaw + altura.
6. **P06** Nicla raw; obtener `w`/score vs distancia real.
7. **P07** centrado visual sin avance.
8. **P08** un globo.
9. **P09** visita + escape + pérdida obligatoria del objetivo.
10. **P10** dos globos idénticos.
11. **P11** cuatro globos idénticos.

## Seguridad

- D2 se mantiene HIGH y reservado para RST del BNO.
- PWM de servos/motores está deshabilitado al boot.
- ARM requiere comando explícito.
- `SAFE_STOP` desarma y desconecta PWM.
- Brushless tienen límite duro de 0.50 del comando normal.
- El software **no** soluciona posible backfeed USB→VCC→boost→VBAT. Antes de dejar actuadores conectados con batería retirada, medir VBAT con solo USB.
