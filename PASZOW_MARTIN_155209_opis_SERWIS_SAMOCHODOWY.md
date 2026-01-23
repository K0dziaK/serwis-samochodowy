# Serwis Samochodowy

## Temat 6 – Serwis samochodów

**Autor:** Martin Paszow (nr. albumu 155209)  
**Repozytorium:** [https://github.com/K0dziaK/serwis-samochodowy](https://github.com/K0dziaK/serwis-samochodowy)

---

## 1. Założenia projektowe

Projekt symuluje działanie serwisu samochodowego zgodnie z tematem 6. Główne założenia:

- **Godziny otwarcia:** Serwis działa od godziny 8:00 do 18:00 (konfigurowalne przez `OPEN_HOUR` i `CLOSE_HOUR`)
- **Obsługiwane marki:** A, E, I, O, U, Y (samogłoski alfabetu)
- **Stanowiska naprawcze:** 8 stanowisk mechaników:
  - Stanowiska 1-7: obsługują marki A, E, I, O, U, Y
  - Stanowisko 8: obsługuje tylko marki U i Y
- **Stanowiska obsługi klientów:** 3 stanowiska (dynamicznie uruchamiane):
  - Stanowisko 1: zawsze aktywne
  - Stanowisko 2: uruchamiane gdy kolejka > K1 (domyślnie 3), zamykane gdy <= 2
  - Stanowisko 3: uruchamiane gdy kolejka > K2 (domyślnie 5), zamykane gdy <= 3
- **Cennik:** 30 różnych usług z określonym kosztem, czasem naprawy i flagą krytyczności
- **Symulacja czasu:** 30 minut symulacji = 1 sekunda rzeczywista

---

## 2. Ogólny opis kodu

### Architektura systemu

Projekt wykorzystuje architekturę wieloprocesową z komunikacją IPC. Każda rola (mechanik, obsługa, kasjer, klient, menedżer) działa jako osobny proces uruchamiany przez `fork()` + `exec()`.

### Struktura plików

| Plik | Opis |
|------|------|
| `main.c` | Główny proces symulacji - inicjalizacja IPC, uruchamianie procesów potomnych |
| `common.h` | Nagłówek z definicjami struktur, stałych i deklaracjami funkcji |
| `common.c` | Implementacja funkcji pomocniczych (wrappery IPC, logowanie, cennik) |
| `role_mechanic.c` | Logika procesu mechanika |
| `role_service.c` | Logika procesu obsługi klienta |
| `role_cashier.c` | Logika procesu kasjera |
| `role_client.c` | Logika procesu klienta |
| `role_manager.c` | Logika procesu menedżera (zarządzanie stanowiskami) |
| `client_generator.c` | Generator procesów klientów |
| `*_main.c` | Pliki wejściowe dla `exec()` poszczególnych ról |

### Przepływ danych

```
Klient → Kolejka MSG → Obsługa → Przypisanie do Mechanika → Naprawa → Kasjer → Klient opuszcza serwis
```

### Mechanizmy IPC

1. **Kolejki komunikatów** - główny mechanizm komunikacji między procesami
2. **Pamięć dzielona** - stan symulacji, statusy mechaników, PID-y procesów
3. **Semafory** - synchronizacja dostępu do zasobów, limity procesów

---

## 3. Zrealizowane funkcjonalności

### Podstawowe wymagania

- [x] Obsługa tylko marek A, E, I, O, U, Y
- [x] 8 stanowisk naprawczych (stanowisko 8 tylko dla U i Y)
- [x] Godziny otwarcia Tp-Tk
- [x] Klienci przychodzą losowo (nawet poza godzinami)
- [x] Oczekiwanie poza godzinami dla usterek krytycznych lub gdy T < T1
- [x] Cennik z 30 usługami
- [x] Akceptacja/odrzucenie warunków przez klienta (~2% odrzuceń)
- [x] Wykrywanie dodatkowych usterek (~20% przypadków)
- [x] Dynamiczne stanowiska obsługi (1-3) w zależności od kolejki
- [x] Sygnały kierownika:
  - Sygnał 1 (SIGUSR1): zamknięcie stanowiska mechanika
  - Sygnał 2 (SIGUSR2): przyspieszenie naprawy o 50%
  - Sygnał 3 (SIGRTMIN): przywrócenie normalnej prędkości
  - Sygnał 4 (SIGTERM): pożar - ewakuacja serwisu

### Wymagania dodatkowe

- [x] Walidacja danych wejściowych
- [x] Obsługa błędów z `perror()` i `errno`
- [x] Minimalne prawa dostępu do struktur IPC (0666)
- [x] Usuwanie struktur IPC po zakończeniu
- [x] Logowanie akcji do pliku `raport.txt`

---

## 4. Napotkane problemy

### Problem 1: Wyścigi przy zamykaniu symulacji
**Opis:** Przy zamykaniu symulacji procesy klientów próbowały wysyłać wiadomości do już usuniętych kolejek.  
**Rozwiązanie:** Implementacja bezpiecznych wrapperów (`safe_msgrcv_wait`, `safe_msgsnd`) sprawdzających `EIDRM` i `EINVAL`, oraz ignorowanie `SIGTERM` przez klientów w trakcie obsługi.

### Problem 2: Deadlock przy obsłudze dodatkowych usterek
**Opis:** Mechanik czekał na odpowiedź od obsługi, która czekała na odpowiedź od klienta.  
**Rozwiązanie:** Wprowadzenie nieblokującego odbierania wiadomości (`IPC_NOWAIT`) dla priorytetowych komunikatów od mechaników.

### Problem 3: Procesy Zombie
**Opis:** Procesy klientów stawały się zombie po zakończeniu.  
**Rozwiązanie:** Dodanie `waitpid()` z `WNOHANG` w głównej pętli oraz prawidłowe zwalnianie semaforów w `client_exit()`.

---

## 5. Elementy wyróżniające

1. **Kolorowane wyjście terminala** - czytelne logowanie z timestampami
2. **Tryb testowy bez sleep** - makro `TESTING_NO_SLEEP` dla szybkiego testowania
3. **Pause/Resume symulacji** - obsługa CTRL+Z (SIGTSTP) i wznowienia (SIGCONT)
4. **Dynamiczne zarządzanie procesami obsługi** przez menedżera
5. **Kompletny cennik usług** z podziałem na kategorie (eksploatacja, naprawy, awarie krytyczne)

---

## 6. Testy

### Test 1: Obsługa nieobsługiwanych marek
**Cel:** Sprawdzenie czy serwis prawidłowo odrzuca marki spoza A, E, I, O, U, Y.  
**Procedura:** Uruchomienie symulacji i obserwacja logów dla klientów z markami B, C, D, itd.  
**Oczekiwany rezultat:** Klienci z nieobsługiwanymi markami są informowani i opuszczają serwis.  
**Status:** ✅

### Test 2: Dynamiczne uruchamianie stanowisk obsługi
**Cel:** Sprawdzenie czy stanowiska 2 i 3 są uruchamiane/zamykane zgodnie z progami K1 i K2.  
**Procedura:** Zwiększenie częstotliwości generowania klientów i obserwacja logów menedżera.  
**Oczekiwany rezultat:** "MANAGER: Uruchamiam Obsługę 2/3" gdy kolejka > K1/K2.  
**Status:** ✅

### Test 3: Sygnał pożaru (sygnał 4)
**Cel:** Sprawdzenie natychmiastowego zamknięcia serwisu.  
**Procedura:** Wprowadzenie "4" w terminalu podczas działania symulacji.  
**Oczekiwany rezultat:** Wszystkie procesy kończą pracę, zasoby IPC są zwalniane.  
**Status:** ✅

### Test 4: Przyspieszenie/przywrócenie prędkości naprawy
**Cel:** Sprawdzenie sygnałów 2 i 3 dla konkretnego mechanika.  
**Procedura:** Wprowadzenie "2 1" (przyspieszenie mechanika 1), potem "3 1" (przywrócenie).  
**Oczekiwany rezultat:** Czas naprawy skrócony o 50%, następnie przywrócony.  
**Status:** ✅

### Test 5: Zamknięcie stanowiska mechanika (sygnał 1)
**Cel:** Sprawdzenie czy mechanik kończy bieżącą naprawę i zamyka stanowisko.  
**Procedura:** Wprowadzenie "1 3" podczas gdy mechanik 3 pracuje.  
**Oczekiwany rezultat:** "Mechanik 3: Zamykam stanowisko po zleceniu."  
**Status:** ✅

### Test 6: Obsługa poza godzinami otwarcia (usterka krytyczna)
**Cel:** Sprawdzenie czy klienci z usterkami krytycznymi czekają poza godzinami.  
**Procedura:** Uruchomienie symulacji przed godziną OPEN_HOUR z wymuszeniem usterki krytycznej.  
**Oczekiwany rezultat:** Klient czeka w kolejce do otwarcia serwisu.  
**Status:** ✅

### Test 7: Działanie programu bez sleepów
**Cel:** Sprawdzenie czy program nie zawiesi się pod dużym obciążeniem.
**Procedura:** Odkomentowanie [#define TESTING_NO_SLEEP](https://github.com/K0dziaK/serwis-samochodowy/blob/main/common.h#L5)
**Oczekiwany rezultat:** Program działa przez dłuższy czas.
**Status:** ✅

---

## 7. Linki do istotnych fragmentów kodu

### a) Tworzenie i obsługa plików
- [`fopen()`, `fprintf()`, `fclose()` - logowanie do raport.txt](https://github.com/K0dziaK/serwis-samochodowy/blob/main/common.c#L55-L73)

### b) Tworzenie procesów
- [`fork()` - tworzenie procesów mechaników](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L118-L130)
- [`execl()` - uruchamianie programów ról](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L125)
- [`exit()` - zakończenie procesu](https://github.com/K0dziaK/serwis-samochodowy/blob/main/role_client.c#L22)
- [`wait()/waitpid()` - oczekiwanie na procesy potomne](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L196-L203)

### c) Obsługa sygnałów
- [`signal()` - ustawienie handlerów](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L66-L68)
- [`sigaction()` - zaawansowana obsługa sygnałów mechanika](https://github.com/K0dziaK/serwis-samochodowy/blob/main/role_mechanic.c#L38-L46)
- [`kill()` - wysyłanie sygnałów do procesów](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L20-L22)
- [`raise()` - wysłanie sygnału do siebie](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L51)

### d) Synchronizacja procesów (semafory)
- [`ftok()` - generowanie klucza IPC](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L72-L74)
- [`semget()` - tworzenie zestawu semaforów](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L78)
- [`semctl()` - inicjalizacja semaforów](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L93-L95)
- [`semop()` - operacje na semaforach (lock/unlock)](https://github.com/K0dziaK/serwis-samochodowy/blob/main/common.c#L170-L178)

### e) Segmenty pamięci dzielonej
- [`shmget()` - tworzenie segmentu](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L77)
- [`shmat()` - dołączanie segmentu](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L81)
- [`shmdt()` - odłączanie segmentu](https://github.com/K0dziaK/serwis-samochodowy/blob/main/role_mechanic.c#L116)
- [`shmctl()` - usuwanie segmentu](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L27)

### f) Kolejki komunikatów
- [`msgget()` - tworzenie kolejki](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L76)
- [`msgsnd()` - wysyłanie wiadomości](https://github.com/K0dziaK/serwis-samochodowy/blob/main/common.c#L119-L129)
- [`msgrcv()` - odbieranie wiadomości](https://github.com/K0dziaK/serwis-samochodowy/blob/main/common.c#L131-L167)
- [`msgctl()` - usuwanie kolejki](https://github.com/K0dziaK/serwis-samochodowy/blob/main/main.c#L26)

---

## 8. Kompilacja i uruchomienie

### Kompilacja
```bash
make clean
make
```

### Uruchomienie
```bash
./simulation
```

### Sterowanie (podczas działania symulacji)
- `1 <id_mechanika>` - zamknięcie stanowiska mechanika
- `2 <id_mechanika>` - przyspieszenie naprawy o 50%
- `3 <id_mechanika>` - przywrócenie normalnej prędkości
- `4` - pożar (natychmiastowe zamknięcie)
- `CTRL+C` - normalne zamknięcie symulacji
- `CTRL+Z` - pauza symulacji
- `fg` - wznowienie symulacji

### Czyszczenie
```bash
make clean
```

---

## 9. Podsumowanie

Projekt realizuje wszystkie wymagania tematu 6 - Serwis samochodowy. Zaimplementowano pełną symulację wieloprocesową z wykorzystaniem mechanizmów IPC (kolejki komunikatów, pamięć dzielona, semafory) oraz obsługę sygnałów. System jest odporny na błędy i prawidłowo zwalnia zasoby przy zamykaniu.
