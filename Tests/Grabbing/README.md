# Grabbing System V1 — prototyp singleplayer

Sterowanie w `B_Character`:

- Przytrzymaj środkowy przycisk myszy: chwyć fizyczny obiekt pod celownikiem.
- Puść przycisk: zwolnij chwyt.
- Podczas chwytu przytrzymaj `R` i poruszaj myszą: obracaj obiekt, z zatrzymanym obracaniem kamery.

`GrabSystem` (`UTOLGrabComponent`) korzysta z istniejących `PhysicsHandle`, `IA_Grabbing` i `IA_Mouse`. Mapowanie `R` powstaje wyłącznie w pamięci podczas gry. Graf postaci i assety wejścia nie są przepisywane. Chwyt nie aktywuje stojących drzew i nie zmienia mechaniki Cutting System.

Konfiguracja komponentu: zasięg chwytu 450 cm, dystans trzymania 120–350 cm, limit masy 250 kg, odległość zerwania chwytu 250 cm. To wartości prototypowe, do dalszego balansu. Masa odniesienia reakcji uchwytu wynosi 5 kg. Cięższe obiekty mają wolniejszą reakcję uchwytu; rzeczywista masa, grawitacja, materiały i wartość obiektu pozostają zachowane.

Publiczne funkcje Blueprint: `TryGrabFromPlayerView`, `TryGrabHit`, `ReleaseGrab`, `SetRotating`, `RotateHeldObject`, `AdjustHoldDistance`. Stan: `HeldComponent`, `HeldMassKg`, `HoldDistance`, `bRotating`, `LastStatus`. Zdarzenie: `OnGrabChanged`. Diagnostyka: `bDrawDebug` i `bShowStatus`.

Zabezpieczenia obejmują pierwszy blokujący obiekt na linii widzenia, masę, zasięg, brak fizyki, zajęty uchwyt, utratę sterowania, zniszczenie obiektu i przebudowę jego fizyki. Podczas chwytu kolizja z Pawn jest czasowo ignorowana; poprzednia odpowiedź wraca po puszczeniu. Pozostałe kolizje pozostają aktywne. Cel przenoszenia jest ograniczany przez test całej obwiedni obiektu względem przeszkód.

## Uruchamianie testów

1. Skompiluj `Tree_of_LegendsEditor` i uruchom mapę `TEST` w singleplayer PIE.
2. Uruchom w konsoli Python edytora plik `Tests/Grabbing/RunPIE.py` z pełnej ścieżki projektu.
3. Poczekaj na `Saved/GrabAcceptanceResults.json`. Każdy zestaw musi mieć `passed: true`.
4. Zakończ PIE, aby usunąć obiekty testowe i przywrócić stan mapy.

Testy przenoszą postać i tworzą obiekty wyłącznie w świecie PIE. Nie zapisują map ani assetów. Sterowanie chwytu i obrotu sprawdzane jest przez wstrzykiwanie wejścia do rzeczywistego Enhanced Input; nie jest to nagranie ręcznej rozgrywki.

- `BasicPIE.py`: chwyt, przenoszenie, obrót, puszczenie, masa, zasięg, przeszkoda, zniszczenie, wyłączenie fizyki i zerwanie chwytu.
- `WallMassPIE.py`: porównanie reakcji 5 kg / 100 kg oraz kontakt przenoszonego obiektu ze ścianą.
- `FragmentsPIE.py`: rzeczywiste odcięte fragmenty Wood i Iron, masa i materiał, obrót przez Enhanced Input, ponowne cięcie i zachowanie sumy wartości.
- `Fixtures.py`: pomocnicze tworzenie i trafianie testowych drzew.

## Ograniczenia

- Prototyp działa tylko w `NM_Standalone`; brak obsługi sieciowej i wspólnego podnoszenia.
- Wartości siły, masy i zasięgu wymagają oceny w rozgrywce. System używa aktualnej masy Chaos; nie dodaje gęstości zależnej od nazwy Wood/Iron/tungsten.
- Obwiednia kolizji celu jest zachowawcza: nieregularne i bardzo długie fragmenty mogą zatrzymywać się wcześniej w ciasnych przejściach.
- Testy nie potwierdzają wszystkich modeli, ekstremalnych prędkości ani wszystkich kontaktów obrotowych. Istniejące otwarte problemy Cutting Physics pozostają osobnym zakresem QA.
- Ponowne cięcie trzymanego fragmentu puszcza chwyt; nie wybiera automatycznie jednego z nowych fragmentów.
- Brak skalowania, przyciągania do siatki, rzutu, zmiany zasięgu kółkiem i docelowego HUD. Zmiana dystansu jest dostępna jako funkcja Blueprint.
