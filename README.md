# goTapFirmware

Firmware for ESP32-S3 tapetårn-display, utviklet som en del av goTapList-økosystemet.

Hver kran på tapetårnet utstyres med en ESP32-S3 1.75" rund AMOLED berøringsskjerm. Skjermen viser logoen til ølen som er koblet til den aktuelle kranen, og lar brukeren registrere tapping direkte fra krandisplayet via goTapList HTTP REST API.

## Funksjonalitet (MVP)
- **Idle / Logovisning:** Henter øllogo, navn, ABV og gjenværende volum fra goTapList-API'et ved oppstart og jevnlig (f.eks. hvert 60. sekund). Viser et "tom kran"-ikon dersom kranen ikke er i bruk.
- **Størrelsesvelger:** Ved trykk på logoen åpnes en overlay/modal med valg for tap-størrelser: **0,25 L**, **0,33 L**, **0,44 L**, og **0,50 L**.
- **Tapping-registrering:** Valgt volum sendes mot API-et (`POST /api/kegs/{kegID}/pour`) med bekreftelsesmelding på skjermen.

## Teknologivalg
- **Språk/Rammeverk:** C++ / Arduino-rammeverket (ESP32-S3).
- **UI-bibliotek:** LVGL (Light and Versatile Graphics Library) for visning av logoer og touch.
- **Nettverk:** Innebygd `WiFi` og `HTTPClient`.
- **Byggesystem:** PlatformIO.

## Bygge- og komme i gang
Prosjektet er strukturert for **PlatformIO**.

Konfigurasjon skjer per enhet:
1. Kopier filen `config.h.example` til `config.h` (denne filen ignoreres av Git for å unngå lekking av innstillinger).
2. Fyll ut dine egne nettverksinnstillinger (WiFi SSID/Passord), goTapList base-URL, og sett spesifikk `TAP_ID` for enheten.
3. Bygg og upload koden ved bruk av PlatformIO IDE i VS Code eller PlatformIO Core (CLI).

## Relaterte linker
- Backend API: [goTapList](https://github.com/MrBoggi/goTapList)
- Frontend: [goTOVGUI](https://github.com/MrBoggi/goTOVGUI)
