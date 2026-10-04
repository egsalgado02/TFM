#!/usr/bin/env python3
"""
gateway_sim.py -- Simulador de gateway LoRa via UART para el TFM.

Habla con la Rev.0 del nodo STM32WL55 a traves del USART1 (PA9/PA10,
expansion) usando un adaptador USB-UART. Sustituye al gateway LoRa
mientras la RF esta indisponible.

Formato de trama (mismo que espera/emite el firmware, ver uart_link.c):

    [SOF=0x7E][FPORT:1B][LEN:1B][payload: LEN bytes]

Uso:
    pip install pyserial
    python gateway_sim.py            (busca COM automatico)
    python gateway_sim.py COM7       (fuerza un COM concreto)
    python gateway_sim.py /dev/ttyUSB0
    python gateway_sim.py --log medidas.csv COM7

Comandos disponibles (escribir y pulsar Enter):
    help                                 muestra la lista de comandos
    force_read <sensor>                  fuerza lectura inmediata (y un flush 2 s despues)
    flush                                el payload builder envia ya lo acumulado
    enable <sensor> <0|1>                habilita/deshabilita sensor
    set_timeout <sensor> <mode> <ms>     periodo de muestreo
    set_threshold <sensor> <mode> <val>  umbral de alarma
    set_battery_th <emerg_mV> <cons_mV>  umbrales de bateria (power_fsm)
    set_window <sensor> <mode> <min> <max>  ventana ADC
    force_mode <mode>                    fuerza estado de energia global
    quit                                 sale del programa

<sensor>: accel | bme | ina | ina_int | adc | payload
<mode>:   abundancia | conservacion | emergencia
"""

import sys
import time
import struct
import threading
from datetime import datetime

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("ERROR: falta pyserial. Instala con:  pip install pyserial")
    sys.exit(1)

# =============================================================
# Constantes que DEBEN coincidir con el firmware
# =============================================================
SOF = 0x7E
FPORT_RUTINA = 1
FPORT_EMERGENCIA = 2
FPORT_COMMAND = 3   # usado por los downlinks; el firmware lo ignora

# Opcodes (de CM4/App/Inc/config_module.h)
OP_SET_TIMEOUT       = 0x01
OP_SET_THRESHOLD     = 0x02
OP_FORCE_READ        = 0x03
OP_SET_BATTERY_TH    = 0x04
OP_ENABLE_SENSOR     = 0x05
OP_SET_WINDOW        = 0x06
OP_FORCE_SYSTEM_MODE = 0x07

# IDs de PAYLOAD (magnitudes que salen del nodo; de sensor_ids.h + accel_fsm.h + bme_fsm.h)
PAYLOAD_ID_NAMES = {
    0:  "accel_x",
    1:  "accel_y",
    2:  "accel_z",
    4:  "bme_temp",
    5:  "bme_hum",
    20: "ina_exp_mA",     # INA238 de expansion (rama mikroBUS, I2C1/PA1)
    21: "ina_exp_mV",
    22: "bme_pres",       # por encima de 15: las decimas de hPa no caben en 12 bits
    23: "adc_batt_mV",    # tension del supercondensador (ADC)
    24: "ina_int_mA",     # INA238 interno (I2C2, PA8)
    25: "ina_int_mV",
}
# IDs empaquetados (<=15) vs planos (>15) en el formato de rutina
PACKED_IDS = {i for i in PAYLOAD_ID_NAMES if i <= 15}
FLAT_IDS   = {i for i in PAYLOAD_ID_NAMES if i > 15}

# IDs de ROUTING (destinos de comandos; de sensor_ids.h)
ROUTE_ID = {
    "accel":   100,
    "bme":     101,
    "ina":     102,   # INA de expansion
    "adc":     103,
    "payload": 104,
    "ina_int": 105,   # INA interno
}

# Power states (de power_fsm.h)
POWER_STATE = {
    "abundancia":   0,
    "conservacion": 1,
    "emergencia":   2,
}

# Emergency codes (de emergency_codes.h)
EMERG_CODES = {
    0x00: "NONE",
    0x01: "ROBO",
    0x02: "CAIDA",
}
EMERG_VALOR_FLAG = 0x80


# =============================================================
# Decoders (tramas del nodo -> humano)
# =============================================================
def decode_rutina(payload: bytes):
    """
    Desempaqueta una trama FPORT_RUTINA. Devuelve lista de
    (id_name, max, min, mean).

    El emisor itera por id ascendente. Los IDs <=15 van empaquetados
    en 5 bytes con id en el nibble alto. Los IDs >15 van en plano en
    7 bytes. Decidimos por catalogo qué tipo es cada bloque.
    """
    out = []
    i = 0
    while i < len(payload):
        first = payload[i]
        packed_id = first >> 4
        flat_id   = first

        if flat_id in FLAT_IDS and i + 7 <= len(payload):
            # Plano 7 bytes
            id_ = flat_id
            max_val = struct.unpack('>h', payload[i+1:i+3])[0]
            min_val = struct.unpack('>h', payload[i+3:i+5])[0]
            mean_val= struct.unpack('>h', payload[i+5:i+7])[0]
            i += 7
        elif packed_id in PACKED_IDS and i + 5 <= len(payload):
            # Packed 5 bytes; valores de 12 bits con signo
            id_ = packed_id
            max_val  = ((first & 0x0F) << 8) | payload[i+1]
            min_val  = (payload[i+2] << 4) | (payload[i+3] >> 4)
            mean_val = ((payload[i+3] & 0x0F) << 8) | payload[i+4]
            # Extension de signo 12 bits -> int16
            if max_val  & 0x800: max_val  -= 0x1000
            if min_val  & 0x800: min_val  -= 0x1000
            if mean_val & 0x800: mean_val -= 0x1000
            i += 5
        else:
            # Byte que no cuadra con el catalogo. Saltar 1 byte y seguir
            # intentando resincronizar dentro de la trama.
            i += 1
            continue

        name = PAYLOAD_ID_NAMES.get(id_, f"id={id_}")
        out.append((name, max_val, min_val, mean_val))
    return out


def decode_emergencia(payload: bytes):
    """Devuelve (sensor_name, emerg_name, valor|None)."""
    if len(payload) < 2:
        return None
    sensor_id = payload[0]
    code = payload[1]
    sensor_name = PAYLOAD_ID_NAMES.get(sensor_id, f"id={sensor_id}")
    emerg_name = EMERG_CODES.get(code & 0x7F, f"UNKNOWN(0x{code:02X})")
    value = None
    if code & EMERG_VALOR_FLAG:
        if len(payload) < 4:
            return None
        value = struct.unpack('>h', payload[2:4])[0]
    return (sensor_name, emerg_name, value)


# =============================================================
# Interpretacion humana de valores por sensor
# =============================================================
# Factores de escala aplicados por payload_builder_fsm (int16 crudos):
#   bme_temp:  decimas de C     -> dividir 10, unidad "°C"
#   bme_hum:   decimas de %RH   -> dividir 10, unidad "%RH"
#   bme_pres:  decimas de hPa   -> dividir 10, unidad "hPa"
#   ina_current_mA:              -> ya en mA, unidad "mA"
#   ina_voltage_mV:              -> ya en mV, unidad "mV"
#   accel_x/y/z:                 -> mg (miligravedades)
_UNITS = {
    "accel_x":        (1.0,   "mg"),
    "accel_y":        (1.0,   "mg"),
    "accel_z":        (1.0,   "mg"),
    "bme_temp":       (0.1,   "°C"),
    "bme_hum":        (0.1,   "%RH"),
    "bme_pres":       (0.1,   "hPa"),
    "ina_exp_mA":     (1.0,   "mA"),
    "ina_exp_mV":     (1.0,   "mV"),
    "ina_int_mA":     (1.0,   "mA"),
    "ina_int_mV":     (1.0,   "mV"),
    "adc_batt_mV":    (1.0,   "mV"),
}


def _prettify(name: str, mx: int, mn: int, avg: int) -> str:
    """Devuelve una cadena tipo 'max=31.4°C  min=31.0°C  mean=31.2°C'."""
    scale, unit = _UNITS.get(name, (1.0, ""))
    def fmt(v: int) -> str:
        if scale == 1.0:
            return f"{v}{unit}"
        return f"{v*scale:.1f}{unit}"
    return f"max={fmt(mx)}  min={fmt(mn)}  mean={fmt(avg)}"


# =============================================================
# Deframer con state machine (bytes -> tramas completas)
# =============================================================
class Deframer:
    S_WAIT_SOF, S_WAIT_FPORT, S_WAIT_LEN, S_WAIT_PAYLOAD = range(4)

    def __init__(self, on_frame):
        self.on_frame = on_frame
        self.state = Deframer.S_WAIT_SOF
        self.fport = 0
        self.length = 0
        self.payload = bytearray()

    def feed(self, byte: int):
        if self.state == Deframer.S_WAIT_SOF:
            if byte == SOF:
                self.state = Deframer.S_WAIT_FPORT
        elif self.state == Deframer.S_WAIT_FPORT:
            self.fport = byte
            self.state = Deframer.S_WAIT_LEN
        elif self.state == Deframer.S_WAIT_LEN:
            self.length = byte
            self.payload = bytearray()
            self.state = Deframer.S_WAIT_SOF if byte == 0 else Deframer.S_WAIT_PAYLOAD
        elif self.state == Deframer.S_WAIT_PAYLOAD:
            self.payload.append(byte)
            if len(self.payload) >= self.length:
                self.on_frame(self.fport, bytes(self.payload))
                self.state = Deframer.S_WAIT_SOF


# =============================================================
# Framer (tramas salientes: opcodes -> bytes)
# =============================================================
def frame(fport: int, payload: bytes) -> bytes:
    if len(payload) > 255:
        raise ValueError("payload > 255 bytes")
    return bytes([SOF, fport & 0xFF, len(payload)]) + payload


# =============================================================
# Interfaz interactiva
# =============================================================
class Gateway:
    def __init__(self, port: str, baud: int = 115200, log_path: str = None):
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.log = open(log_path, "a", buffering=1) if log_path else None
        if self.log:
            self.log.write("# gateway_sim iniciado en {}\n".format(datetime.now()))
        self.deframer = Deframer(self._on_frame)
        self.stop_flag = threading.Event()

    def _on_frame(self, fport: int, payload: bytes):
        import traceback
        ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        try:
            self._on_frame_impl(ts, fport, payload)
        except Exception as e:
            print(f"[ERROR _on_frame]: {e}")
            traceback.print_exc()

    def _on_frame_impl(self, ts: str, fport: int, payload: bytes):
        # Interpretacion humana con unidades. Los valores brutos que
        # empaqueta payload_builder_fsm son int16 sin escalar; aqui les
        # aplicamos las unidades y ponemos el sufijo adecuado por sensor.
        if fport == FPORT_RUTINA:
            entries = decode_rutina(payload)
            if not entries:
                print(f"[{ts}] RUTINA payload no decodificable ({len(payload)}B): "
                      f"{payload.hex(' ')}")
                return
            print(f"[{ts}] === TRAMA RUTINA ({len(payload)}B) ===")
            for name, mx, mn, avg in entries:
                pretty = _prettify(name, mx, mn, avg)
                print(f"           {name:<16} {pretty}")
                if self.log:
                    self.log.write(f"{ts},RUTINA,{name},{mx},{mn},{avg}\n")
        elif fport == FPORT_EMERGENCIA:
            result = decode_emergencia(payload)
            if not result:
                print(f"[{ts}] EMERG payload no decodificable ({len(payload)}B): "
                      f"{payload.hex(' ')}")
                return
            sensor, emerg, val = result
            extra = f"  valor={val}" if val is not None else ""
            print(f"[{ts}] *** EMERGENCIA *** {sensor:<12} {emerg}{extra}")
            if self.log:
                self.log.write(f"{ts},EMERG,{sensor},{emerg},{val}\n")
        else:
            print(f"[{ts}] fport={fport} desconocido ({len(payload)}B): "
                  f"{payload.hex(' ')}")

    def rx_loop(self):
        """Thread background que lee del serial y alimenta el deframer."""
        import traceback
        while not self.stop_flag.is_set():
            try:
                data = self.ser.read(256)
            except (serial.SerialException, OSError):
                print("[serial cerrado]")
                self.stop_flag.set()
                return
            for b in data:
                try:
                    self.deframer.feed(b)
                except Exception as e:
                    # No dejar morir el thread silenciosamente
                    print(f"[ERROR deframer.feed(0x{b:02x})]: {e}")
                    traceback.print_exc()
                    # Resetear estado por si acaso
                    self.deframer.state = Deframer.S_WAIT_SOF

    def send(self, payload: bytes):
        pkt = frame(FPORT_COMMAND, payload)
        self.ser.write(pkt)
        self.ser.flush()
        print(f"[TX] {pkt.hex(' ')}")

    # ---------- Constructores de comandos ----------
    @staticmethod
    def _parse_sensor(s):
        if s not in ROUTE_ID:
            raise ValueError(f"sensor '{s}' desconocido. Opciones: {list(ROUTE_ID)}")
        return ROUTE_ID[s]

    @staticmethod
    def _parse_mode(s):
        if s not in POWER_STATE:
            raise ValueError(f"modo '{s}' desconocido. Opciones: {list(POWER_STATE)}")
        return POWER_STATE[s]

    def cmd_force_read(self, sensor):
        sid = self._parse_sensor(sensor)
        self.send(bytes([OP_FORCE_READ, sid]))
        # El sensor mide, pero su medida se queda acumulada en la ventana del
        # payload builder, que en conservacion dura 30 min, y no se veria.
        # Por eso 2 s despues (tiempo de sobra para que mida y publique) se le
        # pide al payload builder que envie ya lo que tenga. En el firmware,
        # un force_read dirigido al payload builder es justo eso.
        if sensor != "payload":
            threading.Timer(2.0, self.cmd_flush).start()

    def cmd_flush(self):
        """Vacia la ventana de agregacion del payload builder ya."""
        self.send(bytes([OP_FORCE_READ, ROUTE_ID["payload"]]))

    def cmd_enable(self, sensor, en):
        sid = self._parse_sensor(sensor)
        self.send(bytes([OP_ENABLE_SENSOR, sid, 1 if int(en) else 0]))

    def cmd_set_timeout(self, sensor, mode, ms):
        sid = self._parse_sensor(sensor)
        m = self._parse_mode(mode)
        ms = int(ms)
        p = bytes([OP_SET_TIMEOUT, sid, m,
                   (ms >> 16) & 0xFF, (ms >> 8) & 0xFF, ms & 0xFF])
        self.send(p)

    def cmd_set_threshold(self, sensor, mode, val):
        sid = self._parse_sensor(sensor)
        m = self._parse_mode(mode)
        self.send(bytes([OP_SET_THRESHOLD, sid, m, int(val) & 0xFF]))

    def cmd_set_battery_th(self, emerg_mv, cons_mv):
        e = int(emerg_mv); c = int(cons_mv)
        p = bytes([OP_SET_BATTERY_TH,
                   (e >> 8) & 0xFF, e & 0xFF,
                   (c >> 8) & 0xFF, c & 0xFF])
        self.send(p)

    def cmd_set_window(self, sensor, mode, mn, mx):
        sid = self._parse_sensor(sensor)
        m = self._parse_mode(mode)
        mn = int(mn); mx = int(mx)
        p = bytes([OP_SET_WINDOW, sid, m,
                   (mn >> 8) & 0xFF, mn & 0xFF,
                   (mx >> 8) & 0xFF, mx & 0xFF])
        self.send(p)

    def cmd_force_mode(self, mode):
        m = self._parse_mode(mode)
        self.send(bytes([OP_FORCE_SYSTEM_MODE, m]))

    # ---------- Loop principal ----------
    def repl(self):
        HELP = """
Comandos:
  help
  force_read <sensor>      (auto-envia un flush 2 s despues)
  flush                    (vacia la ventana de agregacion ya)
  enable <sensor> <0|1>
  set_timeout <sensor> <mode> <ms>
  set_threshold <sensor> <mode> <val>
  set_battery_th <emerg_mV> <cons_mV>
  set_window <sensor> <mode> <min> <max>
  force_mode <mode>
  quit

<sensor>: accel | bme | ina | ina_int | adc | payload
<mode>:   abundancia | conservacion | emergencia
"""
        print(HELP)
        while not self.stop_flag.is_set():
            try:
                line = input("> ").strip()
            except (EOFError, KeyboardInterrupt):
                break
            if not line:
                continue
            parts = line.split()
            cmd = parts[0].lower()
            args = parts[1:]
            try:
                if cmd in ("quit", "exit"):
                    break
                elif cmd == "help":
                    print(HELP)
                elif cmd == "force_read":
                    self.cmd_force_read(*args)
                elif cmd == "flush":
                    self.cmd_flush()
                elif cmd == "enable":
                    self.cmd_enable(*args)
                elif cmd == "set_timeout":
                    self.cmd_set_timeout(*args)
                elif cmd == "set_threshold":
                    self.cmd_set_threshold(*args)
                elif cmd == "set_battery_th":
                    self.cmd_set_battery_th(*args)
                elif cmd == "set_window":
                    self.cmd_set_window(*args)
                elif cmd == "force_mode":
                    self.cmd_force_mode(*args)
                else:
                    print(f"comando '{cmd}' desconocido. Escribe 'help'.")
            except TypeError as e:
                print(f"argumentos incorrectos: {e}. Escribe 'help'.")
            except ValueError as e:
                print(f"error: {e}")

        self.stop_flag.set()
        self.ser.close()
        if self.log:
            self.log.close()


# =============================================================
# Auto-deteccion de puerto
# =============================================================
def autodetect_port():
    """Busca un adaptador USB-UART candidato entre los puertos disponibles."""
    candidatos = []
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        if any(k in desc for k in ("usb", "ch340", "cp210", "ftdi", "silicon labs",
                                    "prolific", "stlink", "acm", "usbserial")):
            candidatos.append(p.device)
    if not candidatos:
        return None
    if len(candidatos) == 1:
        return candidatos[0]
    print("Multiples puertos serie detectados:")
    for i, p in enumerate(candidatos):
        print(f"  [{i}] {p}")
    idx = input("Elige indice (por defecto 0): ").strip() or "0"
    return candidatos[int(idx)]


# =============================================================
# main
# =============================================================
def main():
    port = None
    log_path = None
    args = sys.argv[1:]
    while args:
        a = args.pop(0)
        if a == "--log" and args:
            log_path = args.pop(0)
        elif a in ("-h", "--help"):
            print(__doc__)
            return
        else:
            port = a

    if port is None:
        port = autodetect_port()
        if port is None:
            print("No detecto ningun puerto serie. Pasa el COM/tty por argumento.")
            print("Ejemplo:  python gateway_sim.py COM7")
            return
        print(f"Puerto detectado automaticamente: {port}")

    try:
        gw = Gateway(port, 115200, log_path)
    except serial.SerialException as e:
        print(f"No se puede abrir {port}: {e}")
        return

    print(f"Escuchando {port} a 115200 8N1. Ctrl+C o 'quit' para salir.\n")
    if log_path:
        print(f"Log CSV: {log_path}\n")

    t = threading.Thread(target=gw.rx_loop, daemon=True)
    t.start()
    gw.repl()


if __name__ == "__main__":
    main()
