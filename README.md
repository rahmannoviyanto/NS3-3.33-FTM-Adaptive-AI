Ini adalah simulasi jaringan wifi dengan topologi
- 1 Node ISP
- 1 Node Router
- 2 Node Access Point (AP)
- 2 Node Station (STA)

Tempatkan file ftm-adaptive-wifi.cc di folder scratch
Tempatkan file ai-analyzer.py di folder root NS3 3.33

running ./waf --run "ftm-adaptive-wifi"

skenario
1. Bandwidth 5 Mbps pada masing-masing AP
2. AP 1 menggunakan jarak statis 5 Meter
3. AP 2 menggunakan jarak dinamis, akan bergerak pada detik ke 5 - 10 sampai sejauh 15 meter, detik 10 - 15 akan stay di 15 meter dan detik 16 - 20 akan mendekat ke jarak 10 meter
4. Lihat stabilitas throghput, RSSI, txPower dll selama 20 Detik di setiap AP
5. Seluruh hasil akan disimpan pada folder result (auto create)
6. Jalankan script python untuk melakukan analisa python3 ftm_ai_analyzer.py
