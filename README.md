# Orbital Odds

Grywalny prototyp 2D: analizujesz dwie losowe kulki, obstawiasz zwycięzcę za wirtualne monety i oglądasz automatyczną walkę.

Interfejs, wyposażenie, zasady i komunikaty gry są teraz w języku angielskim.

## Dźwięki, minki i animacje

- 18 syntetyzowanych efektów dźwiękowych: kliknięcia, odliczanie, start walki, odbicia, broń, trafienia, miny, eksplozje, zdolności, dogrywka i wyniki. Działają lokalnie, bez pobierania plików audio.
- **M** lub przycisk **SFX ON / MUTED** wycisza dźwięk. Suwak obok reguluje głośność. Ustawienia zapisują się w `settings.json`. Bez dostępnego urządzenia dźwiękowego gra nadal działa.
- Każda kulka losuje jedną z **6 odmian oczu**, jedną z **6 buziek** i opcjonalne nakrycie głowy. Są cylinder, korona, czapka zimowa, czapeczka imprezowa i czapka z daszkiem; 40% losowań nie ma nakrycia głowy.
- Kulki mrugają, patrzą na rywala, reagują na trafienia i cieszą się ze zwycięstwa. Ich portrety w kartach odpowiadają wyglądowi na arenie.
- Wygląd jest wyłącznie kosmetyczny i losowany osobnym generatorem. Nie zmienia statystyk, zderzeń, punktów ani wyniku walki.
- Po zatwierdzeniu zakładu pojawia się animowane **3, 2, 1, FIGHT!**. Odliczanie trwa w normalnym tempie przy każdej szybkości symulacji. Fizyka i bronie ruszają dopiero po zakończeniu odliczania. Pauza, katalog i zasady zatrzymują też odliczanie.
- Komunikaty walki wjeżdżają i zanikają, obrażenia unoszą się nad miejscem trafienia, a wynik ma animowane wejście i konfetti. Animacje i ograniczenie częstości dźwięków używają czasu rzeczywistego, żeby pozostały czytelne przy 16×.

## Uruchomienie

Kliknij **`Uruchom.bat`**. Gotowa gra znajduje się w `build/OrbitalOdds.exe`. Plik `data/catalog.json` musi pozostać w folderze projektu. Do grania nie jest potrzebny internet.

Startujesz z 1000 monet. Wybierz zawodnika, ustaw stawkę i kliknij **Place Bet & Fight**. Wypłata przy kursie 1,90 wynosi 190 monet za zakład 100 monet, czyli 90 monet zysku. Kurs jest stały, a nie obliczany z prawdopodobieństwa wygranej. Remis zwraca stawkę.

Zakładka **My Collection** pozwala odblokować do 12 miejsc na własne kulki. Kolejne miejsca kosztują 1000, 2000, 4000 monet itd. **Ball Market** przechowuje trzy oferty, które można dokładnie obejrzeć i kupić; odświeżenie wszystkich ofert kosztuje 1000 monet. Kolekcja i rynek zapisują się w `save.json` i pozostają po rozpoczęciu nowego sezonu. Przycisk deweloperski **Add 10k** można wyłączyć przez ustawienie `ShowDebugAddCoins` na `false` w `src/main.cpp`.

## Co zawiera prototyp

- Dwie bronie, jedna zdolność i dwa modyfikatory losowane dla każdej kulki.
- 6 broni: miecz orbitalny, łuk, ciężki pistolet, miny (25 DMG), rozrzutnik, długa włócznia.
- 6 zdolności: kolce na ścianach, doskok, unik, regeneracja, osłona, wampiryzm.
- 9 modyfikatorów życia, szybkości, rozmiaru, pancerza i obrażeń.
- 5 aren: prostokąt, sześciokąt, ośmiokąt, okrąg i romb.
- Pełny podgląd zestawów przed zakładem; szczegóły po najechaniu na element.
- Katalog w grze z ID i parametrami.
- Pauza i tempo **1× / 2× / 4× / 8× / 16×**.
- Zapis portfela i wyników sezonu w `save.json`.
- Nowy sezon po wyczerpaniu monet.

## Sterowanie

| Klawisz | Działanie |
|---|---|
| Spacja | Rozpoczęcie walki / pauza / wznowienie |
| M | Wyciszenie / włączenie dźwięków |
| 1, 2, 3, 4, 5 | Tempo 1×, 2×, 4×, 8×, 16× |
| R | Nowe losowanie, gdy nie trwa walka |
| F5 | Ponowne wczytanie katalogu i nowe losowanie, poza walką |
| Escape | Zamknięcie katalogu lub zasad |
| Kółko myszy | Przewijanie katalogu |

Otwarcie zasad lub katalogu wstrzymuje walkę. Zamknięcie całej gry podczas walki oznacza utratę postawionej stawki; zapisana stawka nie wraca po ponownym uruchomieniu.

## Edycja balansu

**Edytuj `data/catalog.json`.** Zapisz jako UTF-8 i naciśnij F5 poza walką. Nie trzeba ponownie kompilować gry. Błędny plik wyświetli komunikat; podczas przeładowania gra zachowa poprzedni poprawny katalog.

Przykład wpisu:

```json
{
  "id": "weapon.bow",
  "name": "Łuk",
  "category": "weapon",
  "effect": "bow",
  "points": 25,
  "description": "Celowana strzała co 2 sekundy.",
  "params": {
    "damage": 12,
    "cooldown": 2,
    "projectile_speed": 340,
    "projectile_radius": 3
  }
}
```

| Pole | Znaczenie |
|---|---|
| `id` | Unikalny, stały identyfikator wpisu |
| `name` | Nazwa wyświetlana w grze |
| `category` | `weapon`, `ability` albo `modifier` |
| `effect` | Mechanika obsługiwana przez kod gry |
| `points` | Koszt balansu; może być ujemny dla osłabień |
| `description` | Opis dla gracza; po zmianie parametrów zaktualizuj też opis |
| `group` | Wspólna grupa wyklucza wylosowanie dwóch jej elementów przez tę samą kulkę |
| `params` | Faktyczne obrażenia, odstępy czasu, mnożniki i pozostałe parametry |

Zmiana `points` wpływa na **dobór przeciwników**, a nie na moc broni. Żeby zmienić działanie broni, edytuj `params`. Możesz dodać nowy wariant przez skopiowanie wpisu i nadanie mu nowego `id`. Całkiem nowy rodzaj mechaniki wymaga obsługi w `src/game.cpp`; nieznane `effect` są odrzucane.

Modyfikatory z tej samej grupy nie łączą się: np. podwójne życie i krucha moc mają grupę `hp`. Wartość `armor: 0.2` oznacza redukcję obrażeń o 20%. Mnożniki działają na bazowe statystyki z `rules`. Masa kulki w fizyce rośnie z powierzchnią jej koła. Jednostki rozmiaru odpowiadają jednostkom rysowania areny, prędkości są na sekundę, a czasy w sekundach.

### Algorytm losowania

1. Losuje arenę według jej `weight` oraz cel punktowy z `budget_min`–`budget_max`.
2. Tworzy `candidate_count` losowych zestawów (domyślnie 400).
3. Wybiera zestaw A najbliższy celowi.
4. Szuka zestawu B z różnicą nie większą niż `balance_tolerance` (domyślnie 5 pkt), premiując różnorodność wyposażenia.
5. Losowo zamienia strony i wybiera początkowe kierunki oraz fazy odnowienia broni.

Jeśli po edycji katalogu cel punktowy będzie nieosiągalny, zostanie użyty najbliższy dostępny zestaw. Gdy brak odmiennego zestawu w tolerancji, możliwy jest pojedynek identycznych zestawów. Algorytm wyrównuje koszt, **nie gwarantuje szans 50/50**. Synergie, kontrujące bronie i geometria areny celowo pozostają częścią decyzji gracza. Wartości startowe wymagają dalszego strojenia podczas rozgrywki.

### Areny i przebieg walki

Areny mogą mieć `shape: "rectangle"` lub `shape: "polygon"`. Wielokąt ma `sides` od 4 do 64. Okrąg jest przybliżony 48 bokami. Dopuszczalne rozmiary dla obecnego interfejsu to szerokość 380–600 i wysokość 330–440. `weight` określa częstość losowania.

Pociski i pułapki nie ranią właściciela. Bronie białe mają osobny odstęp między trafieniami. Po odbiciu kulki ze zdolnością kolców na ścianie pozostaje czasowy kolec. Doskok przyspiesza w stronę rywala; unik wykrywa zbliżający się pocisk, przyspiesza w bok i zapewnia krótką nietykalność.

Domyślnie po 55 sekundach zaczyna się dogrywka: obie kulki tracą 3 HP/s z pominięciem pancerza i osłon. Limit walki to 90 sekund. Jeśli do tego czasu obie żyją albo obie zginą w tym samym kroku, jest remis.

Fizyka pracuje ze stałym krokiem 1/120 s, z czterema podkrokami Box2D. Przyspieszenie wykonuje więcej tych samych kroków; nie zwiększa kroku czasu ani obrażeń. Powtarzalność wyników sprawdzana jest dla tego samego zestawu, ziarna i wersji programu. Nie jest obiecywana między różnymi platformami lub wersjami bibliotek.

## Budowanie i testy

Stack: **C++20, raylib 5.5, Box2D 3.1.0, nlohmann/json 3.12.0, CMake**. Przenośny kompilator i CMake w `tools/` są lokalne, bez instalowania narzędzi w systemie.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Test
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Run
```

Skrypt używa krótkiej ścieżki Windows i ustawia `GCC_EXEC_PREFIX`, żeby MinGW poprawnie działał w folderze zawierającym polskie znaki. Jeśli system nie udostępnia krótkich nazw 8.3, należy użyć folderu o ścieżce ASCII lub kompilatora MSVC.

Z własnym kompilatorem i CMake:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

CMake korzysta z bibliotek w `vendor/`; jeśli ich nie ma, pobiera przypięte wersje z oficjalnych repozytoriów. Wymagane są biblioteki systemowe potrzebne raylib dla danego systemu. Interfejs używa systemowej czcionki Segoe UI na Windows, DejaVu Sans na Linux lub Arial na macOS; bez nich działa czcionka zastępcza z ograniczonym zestawem znaków.

`build/OrbitalOdds.exe --smoke` renderuje 10 ekranów do `build/smoke/`: przygotowanie, 3 etapy odliczania, FIGHT!, walkę, wynik, katalog, zestaw wszystkich stylów twarzy i kapeluszy (`styles.png`) oraz zasady. Używa odrębnego portfela testowego. Sprawdza zatrzymanie fizyki podczas odliczania, pauzę, czas odliczania przy 16×, pojedyncze rozliczenie zakładu i odtwarzanie dźwięku przez backend (z wyciszonym wyjściem). Zestaw testów bez okna obejmuje dodatkowo próbki wszystkich dźwięków, 1000 losowań wyglądu, rozliczenia, zapis, pociski, zdolności, remisy, walidację katalogu oraz 150 losowych walk.

## Pliki projektu

- `data/catalog.json` — źródło danych i balansu.
- `src/game.hpp`, `src/game.cpp` — losowanie, fizyka, wyposażenie, portfel i zapis.
- `src/main.cpp` — interfejs i rysowanie gry.
- `src/feedback.hpp`, `src/feedback.cpp` — niezależne losowanie wyglądu, stan odliczania i synteza dźwięków.
- `src/presentation.hpp`, `src/presentation.cpp` — rysowanie minek i kapeluszy oraz odtwarzanie dźwięków.
- `tests/simulation_tests.cpp` — testy logiki bez okna.
- `CMakeLists.txt`, `build.ps1`, `Uruchom.bat` — budowanie i uruchamianie.

To pierwszy prototyp lokalnej gry dla jednego gracza. Nie zawiera płatności, zakładów za prawdziwe pieniądze ani funkcji sieciowych.

Biblioteki: [raylib](https://github.com/raysan5/raylib), [Box2D](https://github.com/erincatto/box2d), [nlohmann/json](https://github.com/nlohmann/json). Ich licencje znajdują się w pobranych katalogach `vendor/`.
