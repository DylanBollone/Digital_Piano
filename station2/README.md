# Station 2 – Audio/Recording Controller

Station 2 serves as the **audio, recording, and system-control station** for the Digital Piano. It receives note information from Station 1 over CAN, generates up to four simultaneous tones using PWM, records and plays back performances, and manages tempo and metronome controls.

## Responsibilities

Station 2 performs the following functions:

- Receives four active note channels over CAN
- Generates up to four simultaneous audio tones
- Converts note values into PWM periods
- Records note changes and their timing
- Reconstructs recorded performances during playback
- Provides recording and playback controls
- Provides adjustable tempo controls
- Enables and disables the metronome
- Sends tempo and metronome information to Station 1
- Displays system controls and status on the LCD

## Audio Generation

Station 2 uses the MC9S12's eight PWM channels as four concatenated channel pairs:

```text
PWM 0 + PWM 1  ──► Audio Channel 1
PWM 2 + PWM 3  ──► Audio Channel 2
PWM 4 + PWM 5  ──► Audio Channel 3
PWM 6 + PWM 7  ──► Audio Channel 4
```

Concatenating each pair provides four independent PWM outputs.

A lookup table maps each received piano-key value to the PWM period required for that note. Each active output uses a 50% duty cycle.

This allows Station 2 to generate up to **four notes simultaneously**.

## CAN Communication

Station 2 communicates bidirectionally with Station 1.

### Note Reception

Station 2 receives CAN messages identified by `N` containing the four active note channels.

```text
Station 1
    │
    │  Four Note Channels
    ▼
Station 2
    │
    ▼
4-Channel PWM Audio
```

The received values are staged and then applied to the four PWM outputs.

### Tempo Transmission

Station 2 sends tempo and metronome information back to Station 1 using CAN messages identified by `B`.

```text
Station 1
    ▲
    │  Tempo / Metronome
    │
Station 2
```

This keeps the Station 1 metronome synchronized with the controls on Station 2.

## Recording System

Station 2 includes an event-based recording system designed to store a performance without continuously saving the state of all four audio channels.

Instead, an event is created when an audio channel changes.

Each event occupies two bytes:

```c
typedef struct
{
    uint8 action;
    uint8 delta;
} event;
```

The `action` value identifies the affected PWM channel and its new note value.

The `delta` value stores the number of timer ticks that elapsed since the previous recorded event.

The program reserves storage for:

```text
2,000 events × 2 bytes/event
```

This event-based approach avoids repeatedly storing unchanged channel states.

## Recording Timing

An ECT timer interrupt provides the time base for recording and playback.

A delta counter increments while recording. When a note changes, the current counter value is stored with the new event and the counter is reset.

```text
Previous Event
     │
     │  delta ticks
     ▼
 Note Change
     │
     ├── Store channel/note
     ├── Store elapsed delta
     └── Reset delta counter
```

Because the delta value is 8 bits, it can represent a maximum of 255 ticks. If the counter reaches 255 without a note change, an additional event is inserted and the counter begins again.

## Playback

Playback processes the recorded events sequentially.

The stored `delta` value determines how long the system waits before applying the next event. When the countdown reaches zero, the stored channel and note information is applied to the appropriate PWM channel.

Events occurring very close together can be processed during the same playback interval, allowing multiple channel changes to be reconstructed together.

## User Interface

Station 2 uses the LCD and four onboard pushbuttons to provide three interface modes.

The PH0 button cycles between the screens.

### Main Screen

Displays navigation information for the interface.

### Recording / Playback Screen

Provides controls for:

```text
PH3 → Record
PH2 → Stop
PH1 → Play
```

The LCD reports the current state, including:

- Waiting
- Recording
- Stopped
- Playing
- Paused

### Tempo / Metronome Screen

Provides controls for:

```text
PH3 → Toggle Metronome
PH2 → Increase Tempo
PH1 → Decrease Tempo
```

Tempo changes occur in increments of **10 BPM**.

The supported range is:

```text
40 BPM – 200 BPM
```

Changes are transmitted to Station 1 over CAN.

## Main Program Flow

```text
Initialize
    │
    ├── Clock
    ├── PWM
    ├── LCD
    ├── ECT
    └── CAN
    │
    ▼
Read User Controls
    │
    ▼
Select Interface Mode
    │
    ├── Main Screen
    ├── Record / Playback
    └── Tempo / Metronome
    │
    ▼
Receive Note Data over CAN
    │
    ▼
Timer Interrupt
    │
    ├── Update PWM Audio
    ├── Record Note Changes
    ├── Process Playback
    └── Maintain Timing
    │
    ▼
Repeat
```

## Peripherals Used

| Peripheral | Purpose |
| --- | --- |
| CAN0 / MSCAN | Receive notes and transmit tempo information |
| PWM | Generate four audio channels |
| ECT | Recording, playback, and system timing |
| LCD / Port K | Display controls and system status |
| Port H | Read user-interface pushbuttons |

## Files

```text
station_2/
├── README.md
├── main.c
├── isr_vectors.c
├── my_vectors.h
├── LCD_driver.c
├── LCD_header.h
└── derivative.h
```

### `main.c`

Contains the primary Station 2 application, including PWM audio generation, CAN communication, recording/playback, tempo controls, LCD interface logic, and timer-driven processing.

### `isr_vectors.c` / `my_vectors.h`

Provide the interrupt-vector configuration used by the application.

### `LCD_driver.c` / `LCD_header.h`

Provide the interface to the Dragon12-Light LCD.

### `derivative.h`

Provides the device-specific definitions for the MC9S12DG256.

## Course-Provided Code

The LCD driver and interrupt-vector infrastructure were provided by **Dr. Andrew Jones** for EGEE 355 at Lake Superior State University and are included with permission.

Original attribution comments have been retained in the corresponding source files.
