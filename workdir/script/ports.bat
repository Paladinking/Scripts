@echo off & python -x %~f0 %* & exit /B
import serial.tools.list_ports

if __name__ == "__main__":
    ports = serial.tools.list_ports.comports()
    print(str([f"{port.description}" for port in ports])[1:-1])


