""" Bar code rotator.  Displays bar code, then blank, then bar caode, etc on Wiegand Tester display"""
import serial
import time

def main():
    print("U and at em")
    ser = serial.Serial("COM6",115200)

    num = 123456
    while True:
        ser.write(f"barcode  {num}\n".encode())
        num += 1
        time.sleep(1)
        ser.write("clear\n".encode())
        time.sleep(2)



if __name__ == "__main__":
    main()