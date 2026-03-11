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
- Eksempel oled skjerm: [goTapOLED](https://www.aliexpress.com/item/1005008995336669.html?spm=a2g0o.productlist.main.30.e47f0VM50VM5QN&algo_pvid=3f4e8b9c-6b0e-4aba-bdd6-13b22a232e2c&pdp_ext_f=%7B%22order%22%3A%2259%22%2C%22eval%22%3A%221%22%2C%22fromPage%22%3A%22search%22%7D&utparam-url=scene%3Asearch%7Cquery_from%3A%7Cx_object_id%3A1005008995336669%7C_p_origin_prod%3A)
