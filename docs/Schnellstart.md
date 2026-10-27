# NegativeAtomium – Schnellstart

Ausführlich: [Handbuch](Handbuch.md).

NegativeAtomium ist ein semi-modularer, paraphoner Analog-Synthesizer. Er spielt **ohne ein einziges
Kabel** – alle Baugruppen sind intern verbunden. Die Patchbay rechts trennt diese Verbindungen auf.

![NegativeAtomium](images/panel.png)

---

## Loslegen

1. NegativeAtomium auf eine **Instrument-/MIDI-Spur** laden und Noten spielen.
2. Oben **PRESET** anklicken und einen Sound laden – oder von Grund auf schrauben.
3. Erst **FREQ** und **RESO** im VCF drehen, dann die beiden Hüllkurven, dann **SHAPE** an den
   Oszillatoren.
4. **SAVE** speichert den Sound samt Patchkabeln.

**Knobs:** ziehen, beim Ziehen erscheint der Wert. **Doppelklick** = Grundwert. **MODE**/**RANGE**
schalten eine Stufe weiter, die LEDs zeigen die Stufe (direkt anklickbar).

Weiße Rahmen = Audioweg, graue Rahmen = Modulation.

## Der Signalweg ohne Kabel

**OSC 1 + OSC 2** (über OSC MIX) **+ NOISE → VCF → OVERDRIVE → VCA → DELAY → OUTPUT**.
Hüllkurve 1 steuert die Lautstärke, Hüllkurve 2 das Filter, der LFO das Filter (MOD DEPTH) und über
ATTENUATOR 2 die Pulsbreite.

## Die Sektionen

| Sektion | Wichtigste Regler |
|---|---|
| **OSC 1 / OSC 2** | **TUNE**, **SHAPE** (Sinus → Dreieck → Säge → Rechteck → Tone Mod), **RANGE** (8', 16', 32', FULL), **WIDTH**, **OSC SYNC**, **PARAPHONIC** |
| **VCF** | **MODE** (LP/BP/HP), **FREQ**, **RESO**, **KEY TRK**, **MOD DEPTH** (LFO), **ENV DEPTH** (Hüllkurve 2) |
| **LFO** | **RATE**, **SHAPE**, **KEY SYNC**, **SYNC** (DAW-Tempo) |
| **NOISE / VCA BIAS** | Rauschen; VCA BIAS öffnet den VCA dauerhaft (Drones) |
| **ENVELOPE 1 · VCA / 2 · VCF** | Je **A D S R** |
| **OVERDRIVE** | **DRIVE**, **TONE**, **LEVEL** |
| **DELAY** | **TIME** (25–640 ms), **REPEATS**, **MIX** |
| **OUTPUT** | **VOLUME**, **ASSIGN** |

## Patchbay

| Aktion | So geht's |
|---|---|
| Kabel stecken | Erst einen **Ausgang** anklicken („SELECT DESTINATION“), dann einen **Eingang** |
| Kabel entfernen | Dieselbe Verbindung noch einmal stecken – oder den Eingang **rechts anklicken** |

Ein Kabel **ersetzt** die interne Verbindung dieses Eingangs, wie eine Klinkenbuchse. Bis zu vier Kabel
pro Eingang werden addiert.

Zum Ausprobieren: **LFO → OSC 2** (Vibrato), **S&H → FREQ MOD** (Zufallsfilter), **ENV 2 → RES**.

## SETTINGS

Die Taste **SETTINGS** öffnet Notenpriorität, Bend Range, Key Split, Drift, LFO-Optionen und mehr.
**CLOSE** schließt.

## Hilfe, es wabert / kein Ton

- **Klang wabert, obwohl MOD DEPTH auf null steht:** ATTENUATOR 2 zudrehen (LFO → Pulsbreite).
- **Kein Ton:** VOLUME prüfen und ob ein Kabel in VCA CV, VCA IN oder VCF IN die interne Verbindung
  ersetzt.
