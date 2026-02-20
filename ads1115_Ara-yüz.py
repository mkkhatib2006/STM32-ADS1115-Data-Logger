import serial
import serial.tools.list_ports
import time
import os
import sys 

def find_stm32_port():
    print("STM32 aranıyor...")
    available_ports = serial.tools.list_ports.comports()
    for port_info in available_ports:
        port = port_info.device
        try:
            
            ser = serial.Serial(port, 115200, timeout=2)
            if ser.readline().strip() == b'READY':
                print(f"*** STM32 BULUNDU: {port} ***")
                return ser
            ser.close()
        except: pass
    return None

def main():
    ser = find_stm32_port()
    if ser is None:
        print("STM32 Bulunamadı! Reset atıp tekrar deneyin."); input(); return

    try:
        print("\n--- ÇALIŞMA MODU ---")
        print("1. Zaman Ayarlı (İstediğiniz SPS Hızında)")
        print("2. Kare Dalga Tetiklemeli (SPS yok, Sinyal Hızında)")
        
        mode = "0"
        sps = "860" 

        while True:
            c = input("Seçim (1/2): ")
            
            if c == "1": 
                mode = "0"
                while True:
                    try:
                        val_str = input("İstediğiniz Hız (1 - 860 SPS arası): ")
                        val = int(val_str)
                        if 1 <= val <= 860:
                            sps = val_str
                            break
                        else:
                            print("HATA: 1-860 arası girin.")
                    except ValueError:
                        print("HATA: Sayı girin.")
                break
                
            elif c == "2": 
                mode = "1"
                sps = "0"
                break

        while True:
            sec_str = input("Süre (sn): ")
            if sec_str.isdigit() and int(sec_str)>0: 
                duration = int(sec_str)
                break

        ser.reset_input_buffer()
        ser.write(f"{mode},{sps},{duration}\n".encode())
        print(f"\nKomut gönderildi. Kayıt Başlıyor...\n")
        
        # Dosya Yolu
        desktop = os.path.join(os.path.expanduser("~"), "Desktop")
        if not os.path.exists(desktop): desktop = os.path.join(os.path.expanduser("~"), "OneDrive", "Desktop")
        filename = os.path.join(desktop, "olcum_sonuclari.txt")

        # --- SAYAÇ DEĞİŞKENLERİ ---
        start_time = 0
        total_samples = 0
        is_running = False

        with open(filename, "w") as f:
            if mode == "1":
                ser.timeout = None
                print("--> Sinyal bekleniyor... (Kare Dalga)")
            else:
                ser.timeout = duration + 5

            while True:
                try:
                    # Veriyi oku
                    line = ser.readline().decode('ascii', errors='ignore').strip()
                except: continue
                
                if not line: 
                    if mode=="0": break
                    else: continue
                
                # Başlangıç Sinyali
                if line.startswith("START"): 
                    f.write(f"--- {line} ---\n")
                    start_time = time.time()
                    is_running = True
                    print(f"BAŞLADI: {line}")
                    print("-" * 50) # Ayırıcı çizgi

                # Bitiş Sinyali
                elif line == "DONE": 
                    # Sayacı temizle ve bitiş mesajı yaz
                    sys.stdout.write(f"\r✅ TAMAMLANDI! Toplam Veri: {total_samples}                    \n")
                    break

                # Bilgi Mesajları
                elif "DEBUG" in line: print(f"\n{line}")
                elif line == "READY": pass
                elif "WAITING" in line: pass

                # VERİ SATIRI GELDİĞİNDE
                else: 
                    # 1. Dosyaya kaydet
                    f.write(f"{line}\n")
                    total_samples += 1

                    # 2. EKRANDA SAYAÇ GÜNCELLE
                    if is_running:
                        elapsed = time.time() - start_time
                        
                        if mode == "0": # Zaman modunda geri sayım
                            remaining = duration - elapsed
                            if remaining < 0: remaining = 0
                            # \r ile satır başına dönüp üzerine yazıyoruz
                            sys.stdout.write(f"\r⏳ Kalan: {remaining:.1f} sn | 📊 Veri: {total_samples}    ")
                        else: # Kare dalga modunda geçen süre
                            sys.stdout.write(f"\r⏱️  Geçen: {elapsed:.1f} sn | 📊 Veri: {total_samples}     ")
                        
                        sys.stdout.flush() # Tamponu boşalt (Anında göster)

    except Exception as e: print(f"\nHATA: {e}")
    finally: ser.close()
    
    print(f"\nDosya kaydedildi: {filename}")
    input("Çıkış için Enter...")

if __name__ == "__main__": main()
