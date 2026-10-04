#!/usr/bin/env python3
"""
sniff_raw.py - Lector crudo del UART, sin deframer ni interpretacion.

Uso:
    python sniff_raw.py [puerto] [segundos]

Defaults:
    puerto   = /dev/cu.usbserial-0001
    segundos = 30

Muestra cada lectura del puerto en hex con timestamp y numero de bytes.
No filtra por SOF, no parsea framing, no interpreta payload. Sirve para
diagnosticar problemas fisicos (baudrate, cable, adaptador) y para ver
que llega realmente sin intermediarios.

Al final imprime total de bytes recibidos, para saber de un vistazo si
el enlace esta transportando algo o esta muerto.
"""

import serial
import sys
import time

DEFAULT_PORT = "/dev/cu.usbserial-0001"
DEFAULT_SECS = 30
BAUD = 115200


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PORT
    secs = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_SECS

    try:
        ser = serial.Serial(port, BAUD, timeout=0.2)
    except serial.SerialException as e:
        print(f"[ERROR] No pude abrir {port} a {BAUD}: {e}")
        sys.exit(1)

    print(f"Escuchando {port} @ {BAUD} durante {secs}s (Ctrl+C corta)...")
    print("-" * 72)

    t_start = time.time()
    t_last  = t_start
    total   = 0
    frames  = 0

    try:
        while (time.time() - t_start) < secs:
            data = ser.read(256)
            if not data:
                continue
            now      = time.time()
            elapsed  = now - t_start
            dt       = now - t_last
            t_last   = now
            total   += len(data)
            frames  += 1
            hex_str  = data.hex(" ")
            print(f"[t={elapsed:6.2f}s +{dt*1000:5.0f}ms] "
                  f"[{len(data):3d}b] {hex_str}")
    except KeyboardInterrupt:
        print("\n[interrumpido]")
    finally:
        ser.close()

    print("-" * 72)
    print(f"Total: {total} bytes en {frames} lecturas "
          f"({secs}s reales={time.time()-t_start:.1f}s)")


if __name__ == "__main__":
    main()
