# Antenna Remote

Ein Antennenumschalter für bis zu acht Antennen auf einem ESP32. Er wählt die Antenne nach der Frequenz des Transceivers (Automatik) oder von Hand, zeigt alles auf einem Touch-Display und lässt sich über WLAN, Bluetooth und eine Weboberfläche bedienen. [SDROxide](https://github.com/3DFabrik/sdroxide) kann ihm die Frequenz direkt über WLAN schicken. Die Firmware ist eine Weiterentwicklung der Antenna Remote II.

Die Firmware-Version steht oben in der Weboberfläche. Ein Release-Build trägt den Tag als Version, ein lokaler Build seine Bauzeit.

## Was dazugehört

- **ESP32** Dev Module, 4 MB Flash. Die Partition `min_spiffs` hält zwei Programme vor, damit ein Update über WLAN möglich ist.
- **Touch-Display** ST7789, 320 × 240, quer eingebaut, mit Touch-Controller.
- **Schieberegister** 74HC164 für die Relais der acht Antennen. Genau ein Ausgang ist aktiv.
- **ICOM-Anschluss** (optional): CI-V an `Serial2` (19200 Baud, GPIO 16 und 17). Der Umschalter liest damit die Frequenz und schaltet auf Wunsch den internen Tuner des Transceivers.

| Funktion | GPIO |
| --- | --- |
| Display MOSI, SCLK, MISO | 4, 18, 19 |
| Display CS, DC, RST | 15, 2, 23 |
| Touch CS | 21 |
| Schieberegister CLR, A/B, CLK | 25, 26, 27 |
| Tuner-Ausgang (extern / intern), Tune-Anforderung | 33, 32 |

Die Display-Pins stehen in `libraries/TFT_eSPI/User_Setup.h`. Beim ersten Start fragt das Display nach der Touch-Kalibrierung.

## Bedienung am Gerät

- **Seite 1** zeigt Frequenz, Band und die Antennenliste. `AUTOMATIC MODE` schaltet zwischen Automatik und Handbetrieb um. Im Handbetrieb wählen UP und DN die Antenne. Hält man `ST` eine Sekunde, wird die aktuelle Antenne für das aktuelle Band gespeichert.
- **Seite 2** (`MISC`) zeigt die Verbindung: Modus, SSID, Status, IP, Signal, ob SDROxide verbunden ist, Antenne und Automatik. Hier wird auch der Verbindungsmodus eingestellt.

## Verbindung: Bluetooth oder WLAN

Der Umschalter läuft entweder mit Bluetooth oder mit WLAN, nie mit beidem. Auf Seite 2 wechselt `BT -> WLAN` (bzw. `WLAN -> BT`) den Modus. Das Gerät startet danach neu, und der Wechsel muss einmal bestätigt werden.

**WLAN einrichten:** Im WLAN-Modus öffnet `WLAN setup` eine Netzliste mit Scan. Das Netz antippen und das Passwort mit der Bildschirmtastatur eingeben (`Show` zeigt es im Klartext). Versteckte Netze gibt es über `HIDDEN`. Der Status auf Seite 2 nennt Fehler im Klartext, zum Beispiel `SSID not found` oder `Wrong password?`. Das Passwort liegt getrennt von der Konfiguration im Flash und wird nie ausgegeben.

Über die serielle Konsole (115200 Baud) geht es auch:

| Befehl | Wirkung |
| --- | --- |
| `<WIFI,ssid,passwort>` | speichert das WLAN (das Passwort darf Kommas enthalten) |
| `<NETMODE,wifi>` oder `<NETMODE,bt>` | Modus wählen, danach Neustart |
| `<WIFISTATUS>` | Modus, SSID, Status, IP und Signal |

## Weboberfläche

Im WLAN-Modus ist der Umschalter unter `http://<IP-Adresse>/` erreichbar. Die Seite passt sich der Fenstergröße an.

- **Main:** Frequenz in einer LCD-Anzeige, eine Taste pro Antenne, `AUTOMATIC`, `INT TUNER` und `TUNING`.
- **Settings:** Antennenanzahl, Namen, externer Tuner pro Antenne, Zuordnung der Bänder 160 bis 4 m und ICOM-Typ. `Reboot` startet neu. `Reset` stellt die Werkseinstellungen her, behält aber den WLAN-Modus.
- **Update:** Firmware über den Browser einspielen, siehe unten.

Es gibt kein Passwort, der Umschalter gehört ins Heimnetz. Änderungen nimmt er nur per POST mit einem eigenen Header an, damit eine fremde Webseite ihn nicht schalten kann.

## SDROxide

In SDROxide unter Settings → Radio den Haken `Use Antenna Remote` setzen und unter Settings → Servers die IP-Adresse des Umschalters eintragen. SDROxide zeigt dort, ob die Verbindung steht, und schickt die Frequenz des Radios. Die Automatik des Umschalters wählt danach die Antenne.

Das Protokoll ist Text über TCP, Port 4540, eine Zeile pro Befehl. Es kommt immer nur ein Client zum Zug. Ein weiterer wird abgewiesen, und ein Client, der länger als fünf Sekunden schweigt, wird getrennt.

| Anfrage | Antwort |
| --- | --- |
| `v` | `AntennaRemote 1` und `RPRT 0` |
| `F <Hz>` | `RPRT 0` (Frequenz des Radios in Hertz) |
| `s` | `ant=<1-8> auto=<0\|1> band=<m> name=<Text>` und `RPRT 0` |
| `M <0\|1>` | `RPRT 0` (Automatik aus oder ein, ab Version 1.1.0) |

Alles andere beantwortet der Umschalter mit `RPRT -1`. SDROxide zeigt die aktive Antenne und einen Schalter für die Automatik in der Kopfleiste (Box `ANT SW`).

## Windows-Programm

Das Windows-Programm Antenna Remote V2 läuft weiter über Bluetooth oder die serielle Schnittstelle, auch mit OmniRig. Die Befehle in der Form `<ANTNUMBER,3,0>` sind unverändert. Antennen und Bandzuordnung lassen sich jetzt aber auch in der Weboberfläche pflegen.

## Firmware einspielen

Die Release-Dateien stehen unter *Releases*:

- `Antenna-Remote_ota.bin` ist das Programm für das **Update im Browser** (Reiter *Update*) oder per `curl`:
  `curl -H "X-AR: 1" -F "firmware=@Antenna-Remote_ota.bin" http://<IP>/update`
  Das Update läuft nur im WLAN-Modus. Vorher sollte nicht gesendet werden, denn der Umschalter startet danach neu.
- `Antenna-Remote.bin` ist das Abbild für **USB**. Es wird ab Adresse 0 geschrieben:
  `esptool.py --chip esp32 write_flash 0x0 Antenna-Remote.bin`

## Selbst bauen

Arduino IDE oder `arduino-cli` mit dem Board-Paket `esp32:esp32` in Version **2.0.17**. Die mitgelieferte TFT_eSPI passt zu dieser Version, die Core-Reihe 3 baut sie nicht.

```
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs --libraries libraries .
```

Die Bibliothek `libraries/TFT_eSPI` enthält die Pinbelegung des Displays. Beim Bauen in der Arduino IDE den Ordner `libraries` als Sketchbook-Bibliothek eintragen oder die Bibliothek dorthin kopieren.

## Release über GitHub

Ein Tag `v…` baut die Firmware auf GitHub und legt ein Release mit beiden Dateien an:

```
git tag v1.0.0
git push origin v1.0.0
```

Alternativ startet man *Actions → Release → Run workflow* und gibt die Version ein. Der Ablauf steht in `.github/workflows/release.yml`.

## Lizenz

MIT, siehe `LICENSE`. Die Bibliothek TFT_eSPI unter `libraries/TFT_eSPI` hat ihre eigene Lizenz (`license.txt`).