import socket
import time

ESP32_IP = "192.168.137.143"  # update this with IP printed on Serial Monitor
UDP_PORT = 4210

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

commands = ['F', 'B', 'L', 'B']

print(f"Sending to {ESP32_IP}:{UDP_PORT}")

for cmd in commands:
    sock.sendto(cmd.encode(), (ESP32_IP, UDP_PORT))
    print(f"Sent: {cmd}")
    time.sleep(1)

sock.close()
print("Done!")
