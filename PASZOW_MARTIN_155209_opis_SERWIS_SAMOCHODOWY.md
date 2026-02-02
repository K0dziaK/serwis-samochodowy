# Serwis Samochodowy

## Temat 6 – Serwis samochodów

**Autor:** Martin Paszow (nr. albumu 155209)  
**Repozytorium:** [https://github.com/K0dziaK/serwis-samochodowy](https://github.com/K0dziaK/serwis-samochodowy)  
**Środowisko Testowe:** Ubuntu

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

### Problem 4: Race-condition przy przypisywaniu mechaników
**Opis:** Gdy program działał bez opóźnień (makro `TESTING_NO_SLEEP`), występowały wyścigi gdzie mechanik dostawał zlecenia zanim zdążył zaktualizować swój status w pamięci dzielonej. Prowadziło to do sytuacji, w której mechanik wykonywał zlecenia z kolejki komunikatów nawet po zamknięciu serwisu - kolejka zawierała komunikaty wysłane do "wolnego" mechanika, który w rzeczywistości był już zajęty.  
**Rozwiązanie:** Wprowadzono semafor binarny `SEM_MECHANIC_ASSIGN` ([common.h#L124](common.h#L124)) synchronizujący sekcję krytyczną przypisywania mechaników. Semafor jest inicjalizowany wartością 1 w [main.c#L152](main.c#L152). W funkcji obsługi klienta ([role_service.c#L186-L216](role_service.c#L186)) przed szukaniem wolnego mechanika pobierany jest semafor (`sem_lock`), następnie status mechanika jest **natychmiastowo aktualizowany** na zajęty (`shm->mechanic_status[i] = 1`) jeszcze przed wysłaniem komunikatu ze zleceniem. Dopiero po oznaczeniu mechanika jako zajętego semafor jest zwalniany, co gwarantuje, że inny proces obsługi nie wyśle zlecenia do tego samego mechanika.

### Problem 5: Ujemna liczba klientów w kolejce
**Opis:** Licznik `clients_in_queue` w pamięci dzielonej potrafił osiągać wartości ujemne. Problem wynikał z nieprawidłowej synchronizacji przy odłączaniu klientów - dekrementacja licznika następowała wielokrotnie lub w niewłaściwym momencie.  
**Rozwiązanie:** Przebudowano system odłączania klientów w funkcji `client_exit()` ([role_client.c#L5-L23](role_client.c#L5)). Funkcja przyjmuje flagę `in_queue` określającą, czy klient faktycznie znajduje się w kolejce i wymaga dekrementacji licznika. Dodatkowo, każda operacja na liczniku jest chroniona semaforem `SEM_LOG`, a przed dekrementacją sprawdzany jest warunek `if (shm->clients_in_queue > 0)`, co zapobiega zejściu poniżej zera. Dekrementacja i zwolnienie semafora kolejki (`SEM_QUEUE`) wykonywane są atomowo w tej samej sekcji krytycznej. Analogiczna logika jest zastosowana w obsłudze klienta ([role_service.c#L73-L74](role_service.c#L73) oraz [role_service.c#L135-L136](role_service.c#L135)).

### Problem 6: Awaria procesu nie kończyła symulacji
**Opis:** Jeśli któryś z procesów potomnych (mechanik, obsługa, kasjer, menedżer lub generator) kończył się z błędem, pozostałe procesy nadal działały, co prowadziło do nieprawidłowego stanu symulacji.  
**Rozwiązanie:** W klasie głównej ([main.c#L6-L8](main.c#L6)) zaimplementowano tablicę `pids[100]` przechowującą wszystkie PID-y procesów potomnych oraz licznik `pid_count`. W głównej pętli symulacji ([main.c#L254-L290](main.c#L254)) dodano skanowanie procesów przy użyciu `waitpid(-1, &status, WNOHANG)`. Dla każdego zakończonego procesu sprawdzany jest kod wyjścia - jeśli `WEXITSTATUS(status) != 0`, ustawiana jest flaga `shm->simulation_running = 0`, co powoduje zakończenie całej symulacji. Dodatkowo logowane są procesy zakończone sygnałem (z pominięciem standardowych sygnałów `SIGTERM` i `SIGKILL` używanych przy normalnym zamykaniu).

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
**Procedura:** Odkomentowanie [#define TESTING_NO_SLEEP](https://github.com/K0dziaK/serwis-samochodowy/blob/master/common.h#L7)
**Oczekiwany rezultat:** Program działa przez dłuższy czas.
**Status:** ✅

### Test 8: Jednorazowe wyrzucanie klientów przy zamknięciu serwisu
**Cel:** Sprawdzenie czy klienci są wyrzucani z kolejki tylko raz przy przejściu ze stanu otwartego na zamknięty, a nie przy każdej iteracji pętli.  
**Procedura:** Wstrzymanie symulacji (CTRL+Z) dokładnie w momencie zamknięcia serwisu (godzina 18:00), odczekanie kilku sekund rzeczywistych, a następnie wznowienie (fg). Alternatywnie: obserwacja logów w nocy - komunikat "Zamykam przyjmowanie klientów" powinien pojawić się tylko raz na dzień.  
**Oczekiwany rezultat:** Po wznowieniu klienci są wyrzuceni jednorazowo (komunikat "Wybiła godzina 18:00. Zamykam przyjmowanie klientów" pojawia się raz). Flaga `clients_dismissed_today` zapobiega wielokrotnemu wyrzucaniu ([role_service.c#L44](role_service.c#L44) oraz [role_service.c#L63-L84](role_service.c#L63)).  
**Status:** ✅

### Test 9: Wykrywanie awarii procesu potomnego
**Cel:** Sprawdzenie czy awaria jednego z procesów (mechanik, obsługa, kasjer) kończy całą symulację.  
**Procedura:** Ręczne zabicie procesu z kodem błędu: `kill -9 <PID_mechanika>` podczas działania symulacji.  
**Oczekiwany rezultat:** Komunikat "[MAIN] Wykryto nieautoryzowane zabicie procesu PID X" i zakończenie symulacji.  
**Status:** ✅

### Test 10: Specjalizacja mechanika 8 (marki U i Y)
**Cel:** Sprawdzenie czy mechanik 8 obsługuje wyłącznie marki U i Y.  
**Procedura:** Obserwacja logów przypisań mechaników dla różnych marek.  
**Oczekiwany rezultat:** Mechanik 8 otrzymuje zlecenia tylko dla marek U i Y, pozostałe marki trafiają do mechaników 1-7.  
**Status:** ✅

### Test 11: Odrzucenie oferty przez klienta
**Cel:** Sprawdzenie obsługi przypadku gdy klient rezygnuje z naprawy po otrzymaniu wyceny.  
**Procedura:** Uruchomienie symulacji i obserwacja logów (szansa rezygnacji ~2%).  
**Oczekiwany rezultat:** Komunikat "Klient X odrzucił koszty", klient opuszcza serwis bez naprawy.  
**Status:** ✅

### Test 12: Brak wolnych mechaników
**Cel:** Sprawdzenie obsługi sytuacji gdy wszystkie stanowiska mechaników są zajęte.  
**Procedura:** Zwiększenie obciążenia (więcej klientów, dłuższe naprawy) lub wyłączenie kilku mechaników sygnałem 1.  
**Oczekiwany rezultat:** Komunikat "BRAK WOLNYCH MIEJSC. Odprawiam klienta X", klient kierowany do kasy z kosztem 0.  
**Status:** ✅

### Test 13: Wykrywanie dodatkowych usterek
**Cel:** Sprawdzenie mechanizmu wykrywania i obsługi dodatkowych usterek podczas naprawy.  
**Procedura:** Uruchomienie symulacji i obserwacja logów mechaników (szansa wykrycia ~20%).  
**Oczekiwany rezultat:** Komunikat o wykryciu dodatkowej usterki, pytanie klienta o zgodę, kontynuacja lub zakończenie naprawy w zależności od decyzji.  
**Status:** ✅

---

## 7. Linki do istotnych fragmentów kodu

### a) Tworzenie i obsługa plików
- [`fopen()`, `fprintf()`, `fclose()` - logowanie do raport.txt](https://github.com/K0dziaK/serwis-samochodowy/blob/master/common.c#L104-L175)

### b) Tworzenie procesów
- [`fork()` - tworzenie procesów mechaników](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L174-L182)
- [`execl()` - uruchamianie programów ról](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L178)
- [`exit()` - zakończenie procesu](https://github.com/K0dziaK/serwis-samochodowy/blob/master/role_client.c#L23)
- [`wait()/waitpid()` - oczekiwanie na procesy potomne](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L254-L290)

### c) Obsługa sygnałów
- [`signal()` - ustawienie handlerów](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L120-L122)
- [`sigaction()` - zaawansowana obsługa sygnałów mechanika](https://github.com/K0dziaK/serwis-samochodowy/blob/master/role_mechanic.c#L55-L62)
- [`kill()` - wysyłanie sygnałów do procesów](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L30)
- [`raise()` - wysłanie sygnału do siebie](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L88)

### d) Synchronizacja procesów (semafory)
- [`ftok()` - generowanie klucza IPC](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L125-L127)
- [`semget()` - tworzenie zestawu semaforów](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L132)
- [`semctl()` - inicjalizacja semaforów](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L149-L152)
- [`semop()` - operacje na semaforach (lock/unlock)](https://github.com/K0dziaK/serwis-samochodowy/blob/master/common.c#L229-L245)

### e) Segmenty pamięci dzielonej
- [`shmget()` - tworzenie segmentu](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L131)
- [`shmat()` - dołączanie segmentu](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L135)
- [`shmdt()` - odłączanie segmentu](https://github.com/K0dziaK/serwis-samochodowy/blob/master/role_mechanic.c#L145)
- [`shmctl()` - usuwanie segmentu](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L38)

### f) Kolejki komunikatów
- [`msgget()` - tworzenie kolejki](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L130)
- [`msgsnd()` - wysyłanie wiadomości](https://github.com/K0dziaK/serwis-samochodowy/blob/master/common.c#L250-L268)
- [`msgrcv()` - odbieranie wiadomości](https://github.com/K0dziaK/serwis-samochodowy/blob/master/common.c#L271-L313)
- [`msgctl()` - usuwanie kolejki](https://github.com/K0dziaK/serwis-samochodowy/blob/master/main.c#L37)

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
