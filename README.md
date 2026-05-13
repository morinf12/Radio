# ESP32-S2 Internet Radio

Boîtier de radio Internet basé sur :

- **Wemos/Lolin S2 Mini** (ESP32-S2FN4R2, 4 MB flash, 2 MB PSRAM)
- **Écran TFT ST7789V3** 240×280 px (SPI)
- **DAC I2S PCM5102**
- **Encodeur rotatif KY-040** (volume + mute par appui court)
- **2 boutons-poussoirs** (station précédente / suivante)

Interface web en français : page principale, configuration Wi-Fi, mise à jour OTA.
Stations Montréal préchargées : **98,5 FM** et **Planète 97,7**.

## Câblage

| Module          | Signal | GPIO ESP32-S2 |
|-----------------|--------|---------------|
| ST7789V3        | SCLK   | 12            |
|                 | MOSI   | 11            |
|                 | RST    | 9             |
|                 | DC     | 7             |
|                 | CS     | 5             |
|                 | BL     | 3 (PWM)       |
| PCM5102         | BCK    | 16            |
|                 | LRC    | 17            |
|                 | DIN    | 18            |
|                 | SCK    | GND           |
| KY-040          | CLK    | 33            |
|                 | DT     | 34            |
|                 | SW     | 35            |
| Bouton « Préc.» |        | 37            |
| Bouton « Suiv.» |        | 38            |

Les broches sont définies dans [config.h](config.h).

## Compilation

```powershell
pio run                 # build
pio run -t upload       # flash
pio device monitor      # serial
```

## Premier démarrage

1. La radio démarre en point d'accès **`Radio`** (mot de passe `radio1234`).
2. Connectez-vous avec un téléphone ou un PC : la page captive s'ouvre.
3. Sélectionnez votre Wi-Fi domestique, entrez le mot de passe, validez.
4. La radio redémarre et se connecte. Trouvez son IP sur le routeur ou via mDNS (`http://radio.local`).
5. Ouvrez l'interface web pour gérer les stations et le volume.

## Contrôles physiques

- **Encodeur tourné** : volume +/-
- **Encodeur appui court** : muet / réactiver (ou démarrer la lecture si arrêtée)
- **Encodeur appui long** : réservé (futur menu)
- **Bouton « Préc. »** : station précédente
- **Bouton « Suiv. »** : station suivante

## Pages web

- `/`        : lecture, volume, gestion des stations, rétroéclairage
- `/wifi`    : scan Wi-Fi, connexion, nom d'hôte, redémarrage
- `/update`  : mise à jour OTA via fichier `.bin`

## Notes

- Le **PSRAM 2 MB** intégré de la S2 Mini est utilisé pour le tampon
  audio (64 KiB). Aucune modification matérielle requise.
- Bibliothèque audio : [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S)
  (schreibfaul1).
