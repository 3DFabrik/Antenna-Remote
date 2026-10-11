# Antenna Remote

Ein Antennenumschalter für bis zu acht Antennen auf einem ESP32. Er wählt die Antenne nach der Frequenz des Transceivers (Automatik) oder von Hand, zeigt alles auf einem Touch-Display und lässt sich über WLAN, Bluetooth und eine Weboberfläche bedienen. [SDROxide](https://github.com/3DFabrik/sdroxide) kann ihm die Frequenz direkt über WLAN schicken. Die Firmware ist eine Weiterentwicklung der Antenna Remote II.

Gedacht ist die Einheit für die Fernumschalter von [Amplitec](https://www.amplitec.hu/r_sw_3000_4_way_eng.html) (SW:3000) mit vier, sechs oder acht Wegen. Sie schaltet positiv und passt damit zu Umschaltern mit gemeinsamer Masse. Die Anzahl der Antennen (bis zu acht) wird in den Einstellungen gewählt.

Die Firmware-Version steht oben in der Weboberfläche. Ein Release-Build trägt den Tag als Version, ein lokaler Build seine Bauzeit.

## Was dazugehört

- **ESP32** Dev Module, 4 MB Flash. Die Partition `min_spiffs` hält zwei Programme vor, damit ein Update über WLAN möglich ist.
- **Touch-Display** ST7789, 320 × 240, quer eingebaut, mit Touch-Controller.
- **Schieberegister** 74HC164 für die Relais der acht Antennen. Genau ein Ausgang ist aktiv.
- **ICOM-Anschluss** (optional): CI-V an `Serial2` (19200 Baud, GPIO 16 und 17). Der Umschalter liest damit die Frequenz eines Icom. Andere Funkgeräte meldet ein `rigctld` im WLAN, siehe *Frequenzquelle*.

| Funktion | GPIO |
| --- | --- |
| Display MOSI, SCLK, MISO | 4, 18, 19 |
| Display CS, DC, RST | 15, 2, 23 |
| Touch CS | 21 |
| Schieberegister CLR, A/B, CLK | 25, 26, 27 |
| Relais externer Tuner, Relais Tune-Anforderung | 33, 32 |

Die Display-Pins stehen in `libraries/TFT_eSPI/User_Setup.h`. Beim ersten Start fragt das Display nach der Touch-Kalibrierung.

## Bedienung am Gerät

- **Seite 1** zeigt Frequenz, Band und die Antennenliste. `AUTOMATIC MODE` schaltet zwischen Automatik und Handbetrieb um. Zwischen diesem Schalter und `MISC` stehen zwei Symbole für die Verbindung: im Bluetooth-Modus das Bluetooth-Zeichen (grün, solange ein Client verbunden ist), im WLAN-Modus die Signalstärke (drei Bögen) und ein Bildschirm-Symbol, das grün wird, solange SDROxide verbunden ist. Die Weboberfläche zeigt dasselbe oben rechts. Im Handbetrieb wählen UP und DN die Antenne. Hält man `ST` eine Sekunde, wird die aktuelle Antenne für das aktuelle Band gespeichert. Im Automatikmodus heißt dieselbe Taste `TU` und startet den Tuner, siehe unten.
- **Seite 2** (`MISC`) zeigt die Verbindung: Modus, SSID, Status, IP, Signal, ob SDROxide verbunden ist, Antenne und Automatik. Hier wird auch der Verbindungsmodus eingestellt.

## Tuner

In den Einstellungen der Weboberfläche lässt sich pro Antenne das Häkchen `External Tuner` setzen. Ist es für die gewählte Antenne gesetzt, schaltet der Umschalter das erste Relais (GPIO 33) für den externen Tuner. Damit ist der Antennentuner des Transceivers abgeschaltet.

Die Taste `TU` (Display, Automatikmodus) und `TUNING` (Weboberfläche) sind nur dann aktiv, wenn die gewählte Antenne diese Markierung hat. Sonst sind sie grau. Ein Druck löst über das zweite Relais (GPIO 32) für genau fünf Sekunden die Tune-Anforderung aus, auch wenn der Finger auf der Taste bleibt. Der Transceiver sendet dann ein Signal mit geringer Leistung, auf das sich der externe Tuner abstimmt. Während der Anforderung ist die Taste rot. Ein erneuter Druck bricht sie ab, ebenso ein Wechsel der Antenne oder des Modus.

## Frequenzquelle und Bänder

In den Einstellungen der Weboberfläche wählt man unter *Frequency source*, woher der Umschalter die Frequenz liest:

- **None:** Er liest keine Frequenz. Sie kann trotzdem vom Windows-Programm mit `<QRG,…>` (USB oder Bluetooth) kommen.
- **Icom CI-V:** Er liest die Frequenz eines Icom über `Serial2`. Der TRX-Typ wird dazu gewählt. Einmal pro Sekunde fragt er auch den Sendezustand ab (CI-V-Befehl `1C 00`). Antwortet das Funkgerät darauf nicht, gibt es keine RX/TX-Anzeige.
- **Hamlib rigctld:** Im WLAN-Modus verbindet er sich mit einem `rigctld` (Hamlib), flrig oder dem Hamlib-Server von SDROxide und fragt zweimal pro Sekunde die Frequenz (`f`) und den Sendezustand (`t`) ab. Host (am besten als IP-Adresse) und Port (Vorgabe 4532) werden eingetragen. Ein Beispiel für Hamlib: `rigctld -m <Modell> -r COM3 -s 19200 -T 0.0.0.0`, die Modellnummer zeigt `rigctld -l`. `rigctld` hat keine Anmeldung und muss im LAN erreichbar sein, deshalb nur im Heimnetz einsetzen. Der Umschalter schickt nur Lesebefehle.
- **SDROxide:** SDROxide verbindet sich mit dem Umschalter (Port 4540) und schickt die Frequenz, siehe unten. Die Frequenz von SDROxide wird nur bei dieser Auswahl übernommen. Die übrigen Befehle (Antenne wählen, Automatik) funktionieren bei jeder Quelle. Den Sendezustand meldet SDROxide mit dem Befehl `P`, siehe Tabelle unten.

Oben rechts in der Weboberfläche steht neben der WLAN-Stärke die gewählte Quelle (`CI-V`, `Hamlib` oder `SDROxide`). Der Punkt davor leuchtet grün, solange die Verbindung steht. Das Display zeigt neben dem Band `RX` (grün) oder `TX` (rot), sobald die Quelle den Sendezustand liefert. Die Weboberfläche zeigt dasselbe oben links in der LCD-Anzeige. Beim Senden kommt dort das SWR dazu: bei `rigctld` per `l SWR`, sofern das Funkgerät es meldet (SDROxides eigener Hamlib-Server tut das nicht), bei Icom CI-V per `15 12` (nach der SWR-Skala des Icom, über 3 nur grob) und bei SDROxide mit dem Befehl `W`.

Auf Seite 2 steht die Quelle in der Zeile `SOURCE`. Auch über die serielle Konsole lässt sie sich setzen: `<freq_src,0>` (keine), `<freq_src,1>` (CI-V), `<freq_src,2>` (rigctld) oder `<freq_src,3>` (SDROxide), dazu `<rig_host,192.168.1.20>` und `<rig_port,4532>`.

**Sendeschutz:** Meldet die Quelle, dass das Funkgerät sendet, schaltet der Umschalter keine Antenne um. Der Wechsel (Automatik, Handbetrieb oder Weboberfläche) wird erst am Ende des Sendens ausgeführt, bis dahin steht `waiting for RX` auf der Webseite. Bricht die Meldung ab, gilt das als Empfang. Der Sendezustand kommt bei `rigctld` vom Befehl `t`, bei CI-V von `1C 00` und bei SDROxide vom Befehl `P`. Meldet eine Quelle ihn nicht, steht das in den Einstellungen und es gibt weder RX/TX-Anzeige noch Schutz.

**Bandgrenzen:** In den Einstellungen unter *Band limits* stehen Unter- und Obergrenze jedes Bandes in kHz. Die Vorgabe ist Region 1. Mit der Auswahl gibt es Voreinstellungen für Region 2 und 3, jeder Wert lässt sich verändern. Steht bei einem Band 0 und 0, ist es abgeschaltet. Liegt die Frequenz in keinem Band, bleibt die Antenne wie sie ist.

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
- **Settings:** Antennenanzahl, Namen, externer Tuner pro Antenne, Zuordnung der Bänder 160 bis 4 m, Bandgrenzen und Frequenzquelle. `Reboot` startet neu. `Reset` stellt die Werkseinstellungen her, behält aber den WLAN-Modus.
- **Update:** Firmware über den Browser einspielen, siehe unten.

Es gibt kein Passwort, der Umschalter gehört ins Heimnetz. Änderungen nimmt er nur per POST mit einem eigenen Header an, damit eine fremde Webseite ihn nicht schalten kann.

## SDROxide

In SDROxide unter Settings → Radio den Haken `Use Antenna Remote` setzen und unter Settings → Servers die IP-Adresse des Umschalters eintragen. Im Umschalter unter *Frequency source* `SDROxide` wählen. SDROxide zeigt dort, ob die Verbindung steht, und schickt die Frequenz des Radios. Die Automatik des Umschalters wählt danach die Antenne.

Das Protokoll ist Text über TCP, Port 4540, eine Zeile pro Befehl. Es kommt immer nur ein Client zum Zug. Ein weiterer wird abgewiesen, solange der erste mindestens einmal in zwei Sekunden fragt. Schweigt er länger, übernimmt ein neuer Client die Verbindung, nach fünf Sekunden Stille wird er getrennt.

| Anfrage | Antwort |
| --- | --- |
| `v` | `AntennaRemote 1` und `RPRT 0` |
| `F <Hz>` | `RPRT 0` (Frequenz des Radios in Hertz) |
| `s` | `ant=<1-8> auto=<0\|1> band=<m> n=<Anzahl> name=<Text>` und `RPRT 0` |
| `M <0\|1>` | `RPRT 0` (Automatik aus oder ein, ab Version 1.1.0) |
| `P <0\|1>` | `RPRT 0` (das Funkgerät empfängt oder sendet, ab Version 1.4.0; sollte bei jeder Änderung und mindestens alle 5 Sekunden kommen) |
| `W <SWR>` | `RPRT 0` (das SWR beim Senden, zum Beispiel `W 1.5`, ab Version 1.4.0; etwa zweimal pro Sekunde, solange gesendet wird) |
| `A <n>` | `RPRT 0` (Antenne `n` wählen und auf Handbetrieb schalten, ab Version 1.2.0) |

Alles andere beantwortet der Umschalter mit `RPRT -1`. SDROxide zeigt die aktive Antenne, einen Schalter für die Automatik und im Handbetrieb Tasten zum Weiterschalten in der Kopfleiste (Box `ANT SW`).

## Windows-Programm

Das Windows-Programm Antenna Remote V2 läuft weiter über Bluetooth oder die serielle Schnittstelle, auch mit OmniRig. Die Befehle in der Form `<ANTNUMBER,3,0>` sind unverändert. Antennen und Bandzuordnung lassen sich jetzt aber auch in der Weboberfläche pflegen.

## Firmware einspielen

Die Release-Dateien stehen unter *Releases*:

- `Antenna-Remote_ota.bin` ist das Programm für das **Update im Browser** (Reiter *Update*) oder per `curl`:
  `curl -H "X-AR: 1" -F "firmware=@Antenna-Remote_ota.bin" http://<IP>/update`
  Das Update läuft nur im WLAN-Modus. Vorher sollte nicht gesendet werden, denn der Umschalter startet danach neu.
  Die Weboberfläche prüft über den Browser (höchstens einmal am Tag) auf GitHub, ob es eine neuere Version gibt, und zeigt dann oben einen Hinweis mit Link zum Reiter *Update*. Dort führt ein Link zu den Dateien des Releases.
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

## Hardware

Die Platine liegt unter `hardware/` (KiCad):

- `kicad/` Projekt (Schaltplan, Leiterplatte), Stückliste und Netzliste
- `production/` Gerber- und Bohrdateien, `BOM.xlsx`
- `docs/` Schaltplan als PDF, Anschlussplan des Amplitec-Kabels
- `parts/` ESP32-DevKitC (Symbol, Footprint, 3D-Modell)
- `3d/` STEP-Modell der Platine

**Anschluss des Antennenumschalters (J1, DB9-Buchse):** Die Platine schaltet positiv. Die Relais des Amplitec-Umschalters hängen mit ihrem gemeinsamen Minus an Masse (Pin 3). Die Belegung folgt dem Plan von Amplitec (`hardware/docs/RSW(4)(6)8A1R_DSUB9_Connectors.pdf`). Antenne *n* schaltet das Relais `RLYn`:

| DB9-Pin | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Signal | RLY8 | RLY7 | GND | RLY6 | RLY5 | RLY4 | RLY3 | RLY2 | RLY1 |

Beim 8-fach-Umschalter sind alle Pins belegt. Der 6-fach braucht Pin 3 und 4 bis 9, der 4-fach Pin 3 und 6 bis 9. Das Kabel hat an beiden Enden einen 9-poligen Stecker (male), der Schirm liegt am Metallgehäuse. Amplitec empfiehlt 0,25 mm² bis 70 m und 0,5 mm² bis 100 m Länge sowie einen Test des fertigen Kabels.

**Bekannte Fehler:**

- Der Buzzer (BZ1) ist im Schaltplan nicht richtig angeschlossen. Er hängt an GPIO 34, und der kann nur Eingang. Der Pin muss getauscht werden. Die Firmware steuert noch keinen Buzzer an.
- Der Buzzer sitzt auf der Leiterplatte zu nah am Rand und muss etwas weiter nach innen.

## Lizenz

MIT, siehe `LICENSE`. Die Bibliothek TFT_eSPI unter `libraries/TFT_eSPI` hat ihre eigene Lizenz (`license.txt`).