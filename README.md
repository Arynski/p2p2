# P2P2

Aplikacja do czatowania peer-to-peer z szyfrowaniem end-to-end. 
Nie wymaga serwera pośredniczącego do przesyłania wiadomości — 
połączenie między użytkownikami jest bezpośrednie dzięki technice 
UDP Hole Punching.

## Jak to działa

Centralny serwer STUN przechowuje listę aktywnych pokojów i pomaga 
dwóm klientom "znaleźć się" w sieci, wymieniając ich adresy IP. 
Po nawiązaniu połączenia serwer nie jest już potrzebny — wiadomości 
płyną bezpośrednio między hostem a peerami. Każda sesja ma unikalny 
klucz szyfrowania wymieniany przy połączeniu (X25519 + ChaCha20).

## Wymagania

- libsodium
- ncurses

# KOMPILACJA I URUCHOMIENIE
```bash
mkdir build
cd build
cmake ..
make
./p2p2 <port>
```

## Uwagi

Działa za większością NAT-ów (Full Cone, Restricted Cone). 
Symetryczny NAT nie jest wspierany. Trzeba mieć jakis serwer 
STUN i na nim jako daemona uruchamiac plik wykonywalny z
build p2p2serwer i w klient/main ustawic adres.
