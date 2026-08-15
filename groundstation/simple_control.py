"""
simple_control.py

Script minimo para hablar con BaseTranseiver.ino (la ESP32 que se queda
conectada por USB a tu compu) y mandarle comandos al blimp por ESP-NOW.

No depende de RobotControl.py original (ese importa comm/Serial.py,
input/JoystickManager.py, robot/RobotMaster.py y Preferences.py, que
NO vinieron incluidos en este repo). Este script habla directo con el
protocolo serial que ya implementa BaseTranseiver.ino:

    'A' + 6 bytes MAC                          -> agrega el blimp como peer ESP-NOW
    'C' + 6 bytes MAC + 13 floats (52 bytes)   -> manda ControlInput.params[13]
    'I'                                        -> pide la ultima telemetria (flag=1) recibida

Requiere: pip install pyserial --break-system-packages
"""

import serial
import struct
import time

from user_parameters import SERIAL_PORT, ROBOT_MACS

BAUD = 115200


def mac_to_bytes(mac_str: str) -> bytes:
    """'48:27:e2:e6:e5:64' -> b'\\x48\\x27\\xe2\\xe6\\xe5\\x64'"""
    return bytes(int(part, 16) for part in mac_str.split(":"))


class BlimpLink:
    def __init__(self, port=SERIAL_PORT, baud=BAUD):
        self.ser = serial.Serial(port, baud, timeout=1)
        time.sleep(2)  # espera a que la ESP32 de la base termine de bootear
        self.ser.reset_input_buffer()
        print(self._read_ack())  # "Transmitter ESP Board"
        print(self._read_ack())  # su propia MAC (no la uses)

    def _read_ack(self, timeout=1.0):
        """Lee una linea de texto, saltandose cualquier 'Telemetry ...' o
        'I2C SCAN ...' que llegue sin que la hayamos pedido (la base los
        imprime solos cada vez que recibe algo del blimp por ESP-NOW,
        de forma independiente a los comandos que mandamos por serial)."""
        end = time.time() + timeout
        while time.time() < end:
            line = self.ser.readline().decode(errors="ignore").strip()
            if not line:
                continue
            if line.startswith("Telemetry") or line.startswith("I2C SCAN"):
                continue
            return line
        return "(sin respuesta)"

    def add_peer(self, mac_str: str):
        self.ser.write(b"A" + mac_to_bytes(mac_str))
        time.sleep(0.1)
        print(self._read_ack())

    def register_ground_station(self, mac_str: str):
        """Le dice al blimp (mac_str) que mande su telemetria a ESTA base.
        Sin este paso el blimp se queda con 'Main Base Not Set' para siempre
        y nunca sabe a quien contestarle (vas a ver 'Peer not found' en su log).
        """
        self.ser.write(b"G" + mac_to_bytes(mac_str))
        time.sleep(0.1)
        print(self._read_ack())

    def send_control(self, mac_str: str, params: list):
        """params debe tener exactamente 13 floats.
        params[0] = flag: 0=off/manual-neutro, 2=modo Nicla (autonomo), 5=stop
        params[1] = fx (avance frontal)
        params[2] = fz (altura)
        params[3] = tx (no usado en control por Nicla)
        params[4] = tz (yaw, lo calcula solo el blimp en modo Nicla)
        params[5..10] = ganancias/params extra (dejar en 0 si no las usas)
        params[11] = 1 para forzar recarga de parametros (paramUpdate) en el blimp
        params[12] = libre
        """
        assert len(params) == 13, "params debe tener 13 elementos"
        payload = b"C" + mac_to_bytes(mac_str) + struct.pack("<13f", *params)
        self.ser.write(payload)
        time.sleep(0.05)
        print(self._read_ack())

    # --- Tipos de dato para set_preference, igual que DataTypes.h del firmware ---
    TYPE_INT = 0x01
    TYPE_FLOAT = 0x02
    TYPE_STRING = 0x03
    TYPE_BOOL = 0x04

    def set_preference(self, mac_str: str, key: str, value, dtype: int):
        """Escribe una preferencia en la memoria flash (NVS) del blimp sin
        tener que re-flashear. Formato exacto que espera parseAndSetPreference()
        en ParamManager.cpp:
            0x68 + datatype(1 byte) + keyLength(1 byte) + key(ascii) + valor
        """
        key_bytes = key.encode("ascii")
        assert len(key_bytes) <= 255, "key demasiado larga"

        if dtype == self.TYPE_INT:
            value_bytes = struct.pack("<i", int(value))
        elif dtype == self.TYPE_FLOAT:
            value_bytes = struct.pack("<f", float(value))
        elif dtype == self.TYPE_BOOL:
            value_bytes = struct.pack("<?", bool(value))
        elif dtype == self.TYPE_STRING:
            value_bytes = str(value).encode("ascii")
        else:
            raise ValueError("dtype invalido")

        payload = bytes([0x68, dtype, len(key_bytes)]) + key_bytes + value_bytes
        self.ser.write(b"D" + mac_to_bytes(mac_str) + payload)
        time.sleep(0.1)
        print(self._read_ack())

    def set_bool(self, mac_str: str, key: str, value: bool):
        """Atajo para preferencias booleanas, como zEn/yawEn/rollEn/pitchEn/rotateEn."""
        self.set_preference(mac_str, key, value, self.TYPE_BOOL)

    def set_float(self, mac_str: str, key: str, value: float):
        """Atajo para preferencias tipo float, como kpyaw/kdyaw/fx_charge/etc."""
        self.set_preference(mac_str, key, value, self.TYPE_FLOAT)

    def get_telemetry(self):
        """Pide la ultima telemetria (flag=1) via el comando binario 'I'.
        OJO: puede chocar con las lineas de 'Telemetry ...' que la base
        imprime sola cada vez que le llega algo del blimp. Para ver datos
        en vivo de forma confiable, mejor usa listen_telemetry() en su lugar.
        """
        self.ser.reset_input_buffer()
        self.ser.write(b"I")
        time.sleep(0.1)
        raw = self.ser.read(6 * 4)
        if len(raw) == 6 * 4:
            try:
                return struct.unpack("<6f", raw)
            except struct.error:
                return None
        return None

    def listen_telemetry(self, seconds=None):
        """Escucha y muestra las lineas 'Telemetry ...' / 'I2C SCAN ...' que
        la base imprime sola (sin pedirlas), en vivo. Ctrl+C para parar.
        """
        end = time.time() + seconds if seconds else None
        while end is None or time.time() < end:
            line = self.ser.readline().decode(errors="ignore").strip()
            if line.startswith("Telemetry") or line.startswith("I2C SCAN"):
                print(line)

    def stop(self, mac_str: str):
        self.send_control(mac_str, [5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
        time.sleep(0.05)
        self.send_control(mac_str, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])

    def start_autonomous_balloon(self, mac_str: str):
        """Activa el modo 'flag=2' que en main.cpp dispara el seguimiento por camara."""
        self.send_control(mac_str, [3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
        time.sleep(0.2)
        self.send_control(mac_str, [2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0])
        time.sleep(0.2)

    def close(self):
        self.ser.close()


if __name__ == "__main__":
    if not ROBOT_MACS:
        raise SystemExit(
            "Pon la MAC de tu blimp en user_parameters.py (ROBOT_MACS)."
        )

    blimp_mac = ROBOT_MACS[0]
    link = BlimpLink()

    print(f"Agregando peer {blimp_mac}...")
    link.add_peer(blimp_mac)

    print("Registrando esta base como destino de telemetria del blimp...")
    link.register_ground_station(blimp_mac)
    time.sleep(0.3)

    print("Forzando recarga de preferencias en el blimp (para que tome la MAC nueva)...")
    link.send_control(blimp_mac, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0])
    time.sleep(0.5)

    # --- Ejemplo: activar el lazo de feedback de yaw (para que gire suave hacia el globo) ---
    # Descomenta las que quieras activar. OJO: zEn usa la altura del barometro,
    # solo actívalo si ya confirmaste lecturas de altura reales y estables.
    print("Activando feedback de yaw...")
    link.set_bool(blimp_mac, "yawEn", True)
    # link.set_bool(blimp_mac, "pitchEn", True)
    # link.set_bool(blimp_mac, "rollEn", True)
    # link.set_bool(blimp_mac, "zEn", True)

    print("Recargando preferencias para que el yawEn tome efecto...")
    link.send_control(blimp_mac, [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0])
    time.sleep(0.5)

    print("Activando modo autonomo de globo (camara)...")
    link.start_autonomous_balloon(blimp_mac)

    print("\nEscuchando telemetria en vivo (Ctrl+C para detener)...\n")
    try:
        link.listen_telemetry()
    except KeyboardInterrupt:
        print("Deteniendo blimp...")
        link.stop(blimp_mac)
        link.close()