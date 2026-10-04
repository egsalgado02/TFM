#!/usr/bin/env python3
"""
sniff.py -- lector crudo del puerto serie, muestra los bytes en hex
en tiempo real. Sirve para verificar si el nodo esta transmitiendo
algo, sin ninguna decodificacion.

Uso:
    python sniff.py                    # autodetecta
    python sniff.py /dev/tty.usbserial-0001
"""
import sys
import serial
import serial.tools.list_ports
from datetime import datetime


def autodetect():
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        if any(k in desc for k in ("usb", "ch340", "cp210", "ftdi", "silicon labs",
                                    "prolific", "usbserial", "acm")):
            return p.device
    return None


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else autodetect()
    if not port:
        print("No detecto puerto. Ejemplo: python sniff.py /dev/tty.usbserial-0001")
        return
    print(f"Escuchando {port} a 115200. Ctrl+C para salir.\n")
    ser = serial.Serial(port, 115200, timeout=0.05)
    try:
        while True:
            data = ser.read(256)
            if data:
                ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                print(f"[{ts}] {len(data):3d} bytes:  {data.hex(' ')}")
    except KeyboardInterrupt:
        print("\nfin")
    finally:
        ser.close()


if __name__ == "__main__":
    main()
