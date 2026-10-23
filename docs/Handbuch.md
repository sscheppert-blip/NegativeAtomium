# NegativeAtomium – Handbuch

*[English version](Manual.md)*

![Die Frontplatte von NegativeAtomium](images/panel.png)

NegativeAtomium ist ein semi-modularer, paraphoner Analog-Synthesizer als Plugin (VST3, CLAP,
AU auf dem Mac) und Standalone-App. Er ist ohne ein einziges Kabel spielbar: Alle Baugruppen sind
intern zu einem klassischen Signalweg verbunden. Mit der Patchbay rechts lässt sich dieser Weg
an 32 Eingängen auftrennen und neu verschalten.

---

## Inhalt

1. [Installation](#1-installation)
2. [Bedienung](#2-bedienung)
3. [Der Signalweg](#3-der-signalweg)
4. [Die Sektionen](#4-die-sektionen)
5. [Patchbay](#5-patchbay)
6. [Settings](#6-settings)
7. [MIDI](#7-midi)
8. [Presets](#8-presets)
9. [Tipps und Fehlersuche](#9-tipps-und-fehlersuche)

---

## 1. Installation

NegativeAtomium gibt es als **AU** (nur macOS), **VST3** und **CLAP** für macOS, Windows und Linux,
dazu eine **Standalone**-App, die ohne DAW läuft. Fertige Pakete liegen bei den
[Releases](https://github.com/sscheppert-blip/NegativeAtomium/releases).

| System | Ordner |
|---|---|
| macOS AU | `~/Library/Audio/Plug-Ins/Components/` |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/` |
| macOS CLAP | `~/Library/Audio/Plug-Ins/CLAP/` |
| Windows VST3 | `C:\Program Files\Common Files\VST3\` |
| Windows CLAP | `C:\Program Files\Common Files\CLAP\` |
| Linux VST3 / CLAP | `~/.vst3/` / `~/.clap/` |

Danach die DAW nach neuen Plugins suchen lassen. NegativeAtomium erscheint als Instrument des
Herstellers **NegativeAtomium**. Beim Aktualisieren das alte Plugin (z. B. den Ordner
`NegativeAtomium.vst3`) vorher löschen.

**Standalone-App:** entpacken und von überall starten – auf dem Mac `NegativeAtomium.app` (z. B. in
die Programme), unter Windows `NegativeAtomium.exe`, unter Linux das Programm `NegativeAtomium`.
Audio- und MIDI-Geräte wählst du unter **Options → Audio/MIDI Settings**.

### Plugins auf macOS freigeben

Jede heruntergeladene Datei bekommt von macOS eine versteckte „Quarantäne“-Markierung. Bei Plugins,
die nicht von Apple notarisiert sind – wie NegativeAtomium und viele kostenlose oder Open-Source-Plugins
– kann das bedeuten:

- eine Meldung, das Plugin „kann nicht geöffnet werden“ oder „ist beschädigt“,
- oder die DAW überspringt es beim Scannen einfach.

**Der allgemeine Weg – klappt für jedes Plugin:** die Markierung einmal im **Terminal** entfernen
(Programme → Dienstprogramme → Terminal), danach die DAW neu starten:

```bash
xattr -dr com.apple.quarantine "/Pfad/zum/Plugin"
```

Tipp: `xattr -dr com.apple.quarantine ` (mit dem Leerzeichen am Ende) tippen, die Plugin-Datei aus dem
Finder ins Terminal-Fenster ziehen – der Pfad wird eingefügt – und Return drücken. `-d` löscht die
Markierung, `-r` schließt alles im Plugin-Paket ein. `xattr -l "/Pfad/zum/Plugin"` zeigt, ob noch eine
Markierung da ist.

**Für NegativeAtomium – alle Formate auf einmal:**

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/NegativeAtomium.component ~/Library/Audio/Plug-Ins/VST3/NegativeAtomium.vst3 ~/Library/Audio/Plug-Ins/CLAP/NegativeAtomium.clap /Applications/NegativeAtomium.app
```

Hast du nur einen Teil der Formate installiert, meldet der Befehl die fehlenden – das ist in Ordnung,
die anderen sind trotzdem freigegeben.

Mach das nur bei Plugins aus Quellen, denen du vertraust: Die Markierung ist die Frage von macOS
„Bist du sicher?“.

---

## 2. Bedienung

- **Knobs:** mit der Maus nach oben/unten oder links/rechts ziehen. Beim Ziehen erscheint der
  Wert. **Doppelklick** setzt den Knob auf seinen Grundwert zurück.
- **Tasten** (z. B. BLEND, KEY TRK): Klick schaltet ein und aus; eingeschaltet leuchtet die Taste.
- **MODE und RANGE:** jeder Druck schaltet eine Stufe weiter, die LEDs daneben zeigen die
  aktuelle Stufe. Die LEDs kann man auch direkt anklicken.
- **Fenstergröße:** an der Ecke rechts unten ziehen (60–250 %). Die Größe wird mit dem Projekt
  gespeichert.
- **Kopfzeile:** PRESET lädt einen gespeicherten Sound, SAVE speichert den aktuellen Sound,
  SETTINGS öffnet die erweiterten Einstellungen. Ein Klick aufs Logo zeigt Version und
  Preset-Ordner.

Rahmen mit **weißer Linie und weißem Namensschild** gehören zum Audioweg, Rahmen mit grauer
Linie zur Modulation.

---

## 3. Der Signalweg

Ohne Kabel läuft das Signal so:

```
OSC 1 ─┐
       ├─ OSC MIX ─┐
OSC 2 ─┘           ├─► VCF ─► OVERDRIVE ─► VCA ─► DELAY ─► OUTPUT
NOISE ─────────────┘    ▲  ▲                ▲  ▲
                 LFO ───┘  └─ ENV 2   ENV 1 ┘  └─ VCA BIAS
```

- Hüllkurve 1 steuert die Lautstärke (VCA), Hüllkurve 2 das Filter.
- Der LFO wirkt über MOD DEPTH aufs Filter und über ATTENUATOR 2 auf die Pulsbreite beider
  Oszillatoren.
- Die ganze Klangerzeugung läuft mit doppelter Abtastrate.

---

## 4. Die Sektionen

### OSC 1 / OSC 2

| Element | Funktion |
|---|---|
| **TUNE** | Verstimmung: ±1 Oktave, in FULL ±10 Oktaven |
| **OSC MIX** | Mischung der beiden Oszillatoren: links nur OSC 1, rechts nur OSC 2 |
| **SHAPE** | Wellenform: Sinus → Dreieck → Sägezahn → Rechteck → Tone Mod. Unter dem Knob steht die aktuelle Form |
| **BLEND** | an: SHAPE überblendet stufenlos zwischen den Formen. Aus: SHAPE springt zwischen den fünf Formen (Punkte an der Skala) |
| **OCTAVE / RANGE** | Fußlage 8', 16', 32' oder FULL (TUNE über ±10 Oktaven). RANGE schaltet weiter, die LEDs zeigen die Lage |
| **WIDTH** | Pulsbreite 1–99 %. Wirkt bei Rechteck und Tone Mod |
| **OSC SYNC** | OSC 2 wird hart auf OSC 1 synchronisiert – stimme OSC 2 mit TUNE für typische Sync-Sounds |
| **PARAPHONIC** | zwei gehaltene Tasten verteilen sich auf die beiden Oszillatoren (gemeinsames Filter und VCA) |

*Tone Mod* ist ein Sägezahn, dessen Rampe von der Pulsbreite geteilt wird – WIDTH verschiebt so
eine Kerbe durchs Obertonspektrum.

### VCF (Filter)

12-dB-Multimodefilter mit Selbstoszillation.

| Element | Funktion |
|---|---|
| **MODE** | schaltet Tiefpass (LP) → Bandpass (BP) → Hochpass (HP) |
| **FREQ** | Grenzfrequenz 10 Hz – 15 kHz |
| **RESO** | Resonanz; weit aufgedreht pfeift das Filter selbst |
| **KEY TRK** | die Grenzfrequenz folgt der gespielten Note |
| **MOD DEPTH** | wie stark der LFO (bzw. das Kabel in FREQ MOD) das Filter bewegt |
| **ENV DEPTH** | wie stark Hüllkurve 2 das Filter öffnet (rechts) oder schließt (links) |

Der Patchbay-Ausgang **VCF 2** liefert gleichzeitig die jeweils andere Filterart: bei LP den
Hochpass, bei BP den Tiefpass, bei HP den Bandpass.

### LFO

| Element | Funktion |
|---|---|
| **RATE** | Geschwindigkeit 0,01 Hz – 10 kHz. Mit SYNC: Notenwert 4/1 … 1/64 |
| **SHAPE** | Sinus → Dreieck → Sägezahn → Rechteck → fallender Sägezahn |
| **KEY SYNC** | der LFO startet mit jeder Note neu |
| **SYNC** | der LFO läuft im Tempo der DAW; RATE wählt den Notenwert, die Phase folgt dem Takt |
| **BLEND** | an: SHAPE überblendet stufenlos; aus: SHAPE springt zwischen den Formen |

Reihenfolge der Formen und Startphase je Knob-Stellung lassen sich in den SETTINGS ändern.

### NOISE / VCA BIAS

| Element | Funktion |
|---|---|
| **NOISE** | Pegel des weißen Rauschens im Mixer |
| **VCA BIAS** | öffnet den VCA unabhängig von der Hüllkurve – für Drones oder um den externen Eingang zu hören |

### ENVELOPE 1 · VCA / ENVELOPE 2 · VCF

Zwei ADSR-Hüllkurven: **A** Attack 0,3 ms – 5 s, **D** Decay 2,4 ms – 10 s, **S** Sustain,
**R** Release 1,5 ms – 6 s. Hüllkurve 1 steuert die Lautstärke, Hüllkurve 2 das Filter (über
ENV DEPTH).

Gebundenes Spielen (legato) startet die Hüllkurven nicht neu – das lässt sich in den SETTINGS
ändern (ENV RETRIG).

### OVERDRIVE

| Element | Funktion |
|---|---|
| **DRIVE** | Verzerrung 0–11 |
| **TONE** | dunkler (links) oder heller (rechts) |
| **LEVEL** | Ausgangspegel der Verzerrerstufe |

### DELAY

Analog nachgebildetes Eimerketten-Delay (BBD): Je länger die Zeit, desto dunkler die Echos.

| Element | Funktion |
|---|---|
| **TIME** | Verzögerung 25–640 ms |
| **REPEATS** | Rückkopplung – wie viele Wiederholungen |
| **MIX** | Anteil des Effekts (ganz links: kein Delay) |

### OUTPUT

| Element | Funktion |
|---|---|
| **VOLUME** | Gesamtlautstärke |
| **ASSIGN** | was der Patchbay-Ausgang ASSIGN liefert: Tonhöhe von OSC 1 oder OSC 2 (1 V/Oktave), Anschlagstärke, Modulationsrad oder Aftertouch |

### SAMPLE & HOLD

| Element | Funktion |
|---|---|
| **RATE** | Takt 0,26–28 Hz, mit dem das Eingangssignal (ohne Kabel: Rauschen) abgetastet wird |
| **GLIDE** | glättet die Sprünge (0,5 ms – 1 s) |

### SLEW RATE LIMITER

| Element | Funktion |
|---|---|
| **SLEW** | wie langsam der Ausgang SLEW einem Signal an SLEW IN folgt (1 ms – 3 s) |
| **PORTA TIME** | Gleiten zwischen den gespielten Noten (Portamento, bis 10 s) |

### ATTENUATORS

| Element | Funktion |
|---|---|
| **1** | Abschwächer 1, bis +4 dB Verstärkung. Ohne Kabel liegt das Signal von ATT 2 an |
| **2** | Abschwächer 2. Ohne Kabel liegt der LFO an, und der Ausgang geht fest auf die Pulsbreite beider Oszillatoren |

---

## 5. Patchbay

Links die 32 **Eingänge (IN)**, rechts die 24 **Ausgänge (OUT)**.

- **Kabel stecken:** erst einen Ausgang anklicken (oben erscheint „SELECT DESTINATION“), dann
  einen Eingang.
- **Kabel entfernen:** dieselbe Verbindung noch einmal stecken, oder den Eingang **rechts
  anklicken** – das entfernt alle Kabel an diesem Eingang.
- Bis zu **vier Kabel** pro Eingang werden addiert.
- Die Kabel werden mit dem Projekt und in Presets gespeichert.

Ein gestecktes Kabel **ersetzt** die interne Verbindung dieses Eingangs – wie eine Klinkenbuchse,
die beim Einstecken den Kontakt trennt. Ohne Kabel gelten diese Verbindungen:

| Eingang | Bekommt ohne Kabel |
|---|---|
| VCF IN | OSC MIX + NOISE (+ externer Eingang) |
| FREQ MOD | LFO, Stärke über MOD DEPTH |
| OD IN | Ausgang des Filters |
| VCA IN | Ausgang des Overdrive |
| VCA CV | Hüllkurve 1 |
| DELAY IN | Ausgang des VCA |
| PW 1, PW 2 | ATT 2 |
| ATT 2 IN | LFO |
| ATT 1 IN | ATT 2 |
| ATT 1 CV | ASSIGN |
| S&H IN | NOISE (S&H CLOCK ersetzt den internen Takt) |
| INVERT IN | Hüllkurve 2 |
| MULT | LFO (an MULT 1 und MULT 2) |
| E. GATE 1 | MIDI-Gate |
| E. GATE 2 | MIDI-Gate bzw. E. GATE 1, wenn dort ein Kabel steckt |

Die übrigen Eingänge (OSC 1, OSC 2, OSC 1+2, SHAPE 1/2, RES, DELAY TIME, LFO RATE/SHAPE/TRIG,
SLEW IN, SUM 1 A/B, SUM 2 A/B) sind ohne Kabel wirkungslos.

### Eingänge

| Buchse | Wirkung |
|---|---|
| OSC 1, OSC 2, OSC 1+2 | Tonhöhe von OSC 1, OSC 2 bzw. beiden (1 V/Oktave) |
| INVERT IN | wird invertiert am Ausgang INVERT ausgegeben |
| SHAPE 1, SHAPE 2 | verschieben die Wellenform |
| PW 1, PW 2 | Pulsbreite |
| VCF IN, OD IN, VCA IN, DELAY IN | Audio-Eingänge der jeweiligen Stufe |
| FREQ MOD | Filtermodulation (Stärke über MOD DEPTH) |
| RES | Resonanz |
| VCA CV | Steuerspannung des VCA |
| DELAY TIME | Delayzeit |
| E. GATE 1, E. GATE 2 | Gate für Hüllkurve 1 bzw. 2 – lässt die Hüllkurven auch ohne MIDI-Note laufen |
| S&H IN, S&H CLOCK | Signal und Takt für Sample & Hold |
| LFO RATE, LFO SHAPE | Geschwindigkeit und Form des LFO |
| LFO TRIG | startet den LFO bei jeder steigenden Flanke neu |
| MULT | Verteiler: das Signal erscheint an MULT 1 und MULT 2 |
| ATT 1 IN, ATT 1 CV, ATT 2 IN | Eingänge der Abschwächer; ATT 1 CV regelt die Verstärkung von ATT 1 |
| SLEW IN | Eingang des Slew Limiters |
| SUM 1 A/B, SUM 2 A/B | werden addiert und an SUM 1 bzw. SUM 2 ausgegeben |

### Ausgänge

OSC 1, OSC 2, OSC MIX, VCF 1, VCF 2, OVERDRIVE, VCA, OUTPUT, NOISE, ENV 1, ENV 2, INVERT, LFO,
LFO UNI (nur positiv, 0…1), S&H, MULT 1, MULT 2, MIDI GATE, ATT 1, ATT 2, SLEW, SUM 1, SUM 2,
ASSIGN.

Tonhöhen-Eingänge und der ASSIGN-Ausgang (OSC 1/2) arbeiten mit 1 V/Oktave; der volle Pegel
eines Patch-Signals entspricht ±5 V. Rückkopplungen (z. B. OUTPUT → VCF IN) sind erlaubt und
bleiben stabil.

### Externer Eingang

Das Plugin hat einen Sidechain-Eingang **„Ext In“**. Audio, das die DAW dorthin schickt, läuft
wie beim Hardware-Eingang durch Mixer, Filter, Overdrive, VCA und Delay. Zum Hören den VCA mit
VCA BIAS öffnen oder Noten spielen.

---

## 6. Settings

Die Taste **SETTINGS** in der Kopfzeile öffnet Optionen, die man am Original nur über
Tastenkombinationen erreicht. **CLOSE** schließt sie wieder.

| Option | Funktion |
|---|---|
| **PRIORITY** | welche Note bei mehreren gehaltenen Tasten spielt: LAST (zuletzt), HIGH (höchste), LOW (tiefste), HIGH/LOW (paraphon: tiefste auf OSC 1, höchste auf OSC 2) |
| **BEND RANGE** | Pitch-Bend-Bereich 0–24 Halbtöne |
| **KEY SPLIT** | Tastatur teilen: unterhalb der Split-Note spielt OSC 1, ab ihr OSC 2 |
| **ENV RETRIG** | Hüllkurven starten auch bei gebundenem Spiel neu |
| **PEDAL** | OSC 2 bleibt auf der Note, die vor dem Einschalten gespielt wurde (Orgelpunkt) |
| **MUTE C1–C7 OUT** | Noten außerhalb C1–C7 stumm statt in den Bereich verschoben |
| **AUTOGLIDE 1 / 2** | jede Note gleitet von bis zu ±12 Halbtönen daneben in die Zieltonhöhe (Tempo über PORTA TIME) |
| **DRIFT** | analoges Wandern der Tonhöhe |
| **TUNE BYPASS** | TUNE von OSC 1 bzw. OSC 2 wird ignoriert – exakte Stimmung |
| **DEPTH** (LFO) | Gesamtstärke des LFO |
| **KEY RATE** | LFO wird ab der eingestellten Note pro Oktave doppelt so schnell |
| **ONE SHOT** | LFO läuft nach dem Start nur einen Durchgang (braucht KEY SYNC) |
| **RETRIG** (LFO) | KEY SYNC startet den LFO auch bei gebundenem Spiel neu |
| **VCF MOD / AMOUNT** | Anschlagstärke, Modulationsrad oder Aftertouch öffnet zusätzlich das Filter |
| **ASSIGN OUT** | wie der ASSIGN-Wahlschalter in OUTPUT |
| **LFO SHAPE ORDER / START PHASE** | welche Wellenform an jeder der fünf SHAPE-Stellungen liegt und mit welcher Phase sie startet |

---

## 7. MIDI

- Das Plugin hört auf alle MIDI-Kanäle.
- Noten werden in den Bereich **C1–C7** gelegt (tiefere/höhere Noten werden in den Bereich
  verschoben, siehe auch MUTE C1–C7 OUT).
- **Pitch Bend** (Bereich in den SETTINGS), **Modulationsrad** (CC 1), **Aftertouch** und
  **Sustain-Pedal** (CC 64) werden ausgewertet.
- Die Anschlagstärke wirkt nur, wo sie zugewiesen ist: über ASSIGN oder VCF MOD.
- Alle Knobs und Tasten lassen sich in der DAW automatisieren.

---

## 8. Presets

**SAVE** speichert den aktuellen Sound inklusive aller Patchkabel unter einem Namen, **PRESET**
lädt ihn wieder.

Presets sind einzelne `.napreset`-Dateien und lassen sich zwischen Rechnern kopieren:

| System | Ordner |
|---|---|
| Windows | `%APPDATA%\NegativeAtomium\Presets` |
| macOS | `~/Library/Application Support/NegativeAtomium/Presets` |
| Linux | `~/.local/share/NegativeAtomium/Presets` (oder `$XDG_DATA_HOME/NegativeAtomium/Presets`) |

Unabhängig davon speichert dein DAW-Projekt immer den aktuellen Sound mit.

---

## 9. Tipps und Fehlersuche

- **Der Klang wabert, obwohl MOD DEPTH auf null steht?** Dann ist meist **ATTENUATOR 2**
  aufgedreht: Der LFO moduliert darüber fest verdrahtet die Pulsbreite beider Oszillatoren
  (hörbar bei Rechteck und Tone Mod). ATT 2 zudrehen – oder ein Kabel in PW 1/PW 2 stecken.
- **Kein Ton?** VCA BIAS und VOLUME prüfen, und ob ein Kabel in VCA CV, VCA IN oder VCF IN die
  interne Verbindung ersetzt.
- **Die Tonhöhe wandert leicht?** Das ist DRIFT in den SETTINGS – auf null für eine stabile
  Stimmung.
- **Filter pfeift?** RESO zurückdrehen; ganz aufgedreht oszilliert das Filter selbst.
- **Bass-Drone:** VCA BIAS aufdrehen, OSC 2 eine Oktave tiefer (RANGE), langsamen LFO über
  MOD DEPTH aufs Filter.
- **Sequenz aus Zufall:** S&H auf FREQ MOD stecken, MOD DEPTH aufdrehen, S&H RATE im Tempo
  einstellen.
