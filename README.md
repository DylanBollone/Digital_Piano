# Digital Piano

A two-microcontroller embedded digital piano featuring a 32-key keyboard, four-note polyphony, bidirectional CAN communication, PWM audio generation, adjustable tempo, a metronome, and memory-efficient recording and playback.

Developed as the final project for **EGEE 355 – Microcontroller Systems** at Lake Superior State University.

## Overview

The Digital Piano is a distributed embedded system built around two MC9S12DG256 microcontrollers.

The system is divided into two stations connected through a CAN bus:

- **Station 1 – Keyboard/Input Controller:** Reads the 32-key keyboard over SPI, identifies up to four simultaneous key presses, displays the active notes, and transmits note data to Station 2.
- **Station 2 – Audio/Recording Controller:** Receives note data over CAN, generates up to four simultaneous tones using PWM, and provides recording, playback, tempo, and metronome controls.

Communication is bidirectional. Station 1 sends active note information to Station 2, while Station 2 sends tempo and metronome information back to Station 1.

## System Architecture

```text
                         ┌─────────────────┐
                         │  32-Key Piano   │
                         │    Keyboard     │
                         └────────┬────────┘
                                  │
                             SPI / Shift
                              Registers
                                  │
                                  ▼
                         ┌─────────────────┐
                         │    Station 1    │
                         │                 │
                         │  Key Scanning   │
                         │  Note Display   │
                         │  Metronome      │
                         └────────┬────────┘
                                  │
                          Bidirectional CAN
                    Note Data ↓       ↑ Tempo Data
                                  │
                                  ▼
                         ┌─────────────────┐
                         │    Station 2    │
                         │                 │
                         │   PWM Audio     │
                         │ Record/Playback │
                         │ Tempo Controls  │
                         └────────┬────────┘
                                  │
                           4 PWM Channels
                                  │
                                  ▼
                         ┌─────────────────┐
                         │  Audio Output   │
                         └─────────────────┘
```

## Features

- 32-key piano keyboard
- Up to four simultaneously played notes
- SPI-based keyboard scanning
- Four-channel PWM audio generation
- Bidirectional CAN communication between microcontrollers
- Event-based recording and playback
- Adjustable tempo from 40–200 BPM
- Metronome functionality
- LCD user interfaces
- Interrupt-driven timing
- Button-based recording, playback, and tempo controls

## Station 1 – Keyboard/Input Controller

Station 1 serves as the primary keyboard interface.

Four shift registers are read using the MC9S12's SPI peripheral, allowing the state of all 32 piano keys to be acquired efficiently. The software scans the resulting 32 bits and identifies up to four simultaneously pressed keys.

The detected keys are converted into note values and transmitted to Station 2 over CAN.

Station 1 also displays the currently active notes and system tempo on an LCD. A PWM output and timer interrupt provide local metronome functionality.

### Station 1 Responsibilities

- Read 32 piano keys through SPI
- Detect up to four simultaneous key presses
- Convert keyboard positions into note values
- Display active notes on the LCD
- Transmit four note channels over CAN
- Receive tempo and metronome information from Station 2
- Generate the metronome using PWM and timer interrupts

## Station 2 – Audio/Recording Controller

Station 2 is responsible for audio generation and the higher-level piano controls.

Note data received from Station 1 is mapped through a lookup table containing the PWM periods required for each piano note. Four pairs of PWM channels are concatenated to create four independent 16-bit PWM outputs, allowing up to four notes to be generated simultaneously.

Station 2 also provides an LCD-based user interface for recording/playback and tempo control.

### Station 2 Responsibilities

- Receive active notes from Station 1 over CAN
- Generate four simultaneous PWM audio channels
- Record and play back performances
- Adjust tempo between 40 and 200 BPM
- Enable or disable the metronome
- Display system status on the LCD
- Send tempo and metronome updates to Station 1

## Recording and Playback

Because the microcontroller has limited available memory, continuously sampling and storing the state of every piano channel would be inefficient.

Instead, the recording system uses an **event-based representation**. A new event is stored only when the state of a PWM channel changes.

Each recorded event contains two bytes:

```c
typedef struct
{
    uint8 action;
    uint8 delta;
} event;
```

The `action` byte encodes the PWM channel and note value, while `delta` stores the number of timing ticks since the previous event.

```text
action
┌───────┬────────┬─────────────────────┐
│  7–6  │   5    │        4–0          │
├───────┼────────┼─────────────────────┤
│Channel│ On/Off │     Note Value      │
└───────┴────────┴─────────────────────┘

delta
┌──────────────────────────────────────┐
│       Ticks Since Previous Event     │
└──────────────────────────────────────┘
```

The implementation reserves space for up to **2,000 events**.

If more than 255 timing ticks occur without another note change, an additional event is inserted so that longer periods can still be represented with an 8-bit delta value.

During playback, the stored events are processed in sequence. Their delta values reconstruct the timing between note changes, while the encoded channel and note values reproduce the original PWM output.

This approach significantly reduces memory usage when compared with continuously storing all four channel states.

## CAN Communication

The two stations communicate using the MC9S12's MSCAN peripheral.

### Station 1 → Station 2

Station 1 transmits the state of the four active note channels.

```text
Station 1
    │
    │  CAN message: Note data
    ▼
Station 2
```

Station 2 receives these values and uses them to update its four PWM audio channels.

### Station 2 → Station 1

Station 2 transmits tempo and metronome updates back to Station 1.

```text
Station 1
    ▲
    │  CAN message: Tempo / Metronome
    │
Station 2
```

This bidirectional communication allows the two microcontrollers to operate as a coordinated system while separating keyboard acquisition from audio generation and recording.

## PWM Audio Generation

Station 2 uses all eight hardware PWM channels as four concatenated channel pairs:

```text
PWM 0 + PWM 1  → Audio Channel 1
PWM 2 + PWM 3  → Audio Channel 2
PWM 4 + PWM 5  → Audio Channel 3
PWM 6 + PWM 7  → Audio Channel 4
```

A lookup table maps each piano key to the appropriate PWM period. The output uses a 50% duty cycle, while changing the PWM period changes the generated frequency.

This provides four independent tone generators for four-note polyphony.

## User Interface

Station 2 provides three LCD interface modes controlled using the board's pushbuttons.

### Main Screen

Provides basic navigation information.

### Recording / Playback

Provides controls to:

- Start recording
- Stop recording
- Play the recorded sequence

The LCD displays the current state, such as recording, stopped, or playing.

### Tempo / Metronome

Provides controls to:

- Decrease tempo by 10 BPM
- Increase tempo by 10 BPM
- Enable or disable the metronome

Tempo is limited to a range of **40–200 BPM**.

## Technologies

- MC9S12DG256 Microcontroller
- Embedded C
- CodeWarrior 5.9
- CAN / MSCAN
- SPI
- PWM
- Enhanced Capture Timer (ECT)
- Hardware interrupts
- Parallel LCD interface
- Shift registers
- Event-based data encoding

## Repository Structure

```text
Digital_Piano/
│
├── README.md
│
├── station_1/
│   ├── main.c
│   ├── isr_vectors.c
│   ├── my_vectors.h
│   ├── LCD_driver.c
│   ├── LCD_header.h
│   └── derivative.h
│
└── station_2/
    ├── main.c
    ├── isr_vectors.c
    ├── my_vectors.h
    ├── LCD_driver.c
    ├── LCD_header.h
    └── derivative.h
```

## Course-Provided Code

Some low-level support code used by this project was provided by **Dr. Andrew Jones** for EGEE 355 at Lake Superior State University and is included with permission.

This includes portions of the LCD driver and interrupt-vector infrastructure. Original attribution comments have been retained in the corresponding source files.

The Digital Piano application logic, system integration, CAN communication, keyboard interface, PWM audio system, recording/playback implementation, and user-interface functionality were developed as part of the student final project.

## Authors

**Dylan Bollone**  
**Jess Besonen**  
**Will Peltier**  
**Jadon Lawlor**

EGEE 355 – Microcontroller Systems  
Lake Superior State University  
Spring 2026
