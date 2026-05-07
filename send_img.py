import serial
import time
from PIL import Image

SERIAL_PORT = '/dev/ttyUSB1' 
BAUD_RATE = 115200

def send_image(file_path):
    try:
        img = Image.open(file_path).convert('RGB').resize((96, 64))
        
        img_bytes = []
        for y in range(64):
            for x in range(96):
                r, g, b = img.getpixel((x, y))
                val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                img_bytes.append((val >> 8) & 0xFF) 
                img_bytes.append(val & 0xFF)        

        with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
            print(f"Trimit {len(img_bytes)} bytes catre {SERIAL_PORT}...")
            
            time.sleep(1) 
            
            chunk_size = 16 
            for i in range(0, len(img_bytes), chunk_size):
                chunk = bytearray(img_bytes[i:i+chunk_size])
                ser.write(chunk)
                time.sleep(0.002) 
                
            print("Gata! Vezi ecranul OLED.")

    except Exception as e:
        print(f"Eroare: {e}")

if __name__ == "__main__":
    send_image("poza.jpg") 
