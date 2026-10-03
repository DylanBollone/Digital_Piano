# Station 1 – Keyboard/Input Controller

Station 1 serves as the **keyboard and input controller** for the Digital Piano. It reads the 32-key keyboard, determines which notes are being played, displays system information on the LCD, and communicates with Station 2 over CAN.

## Responsibilities

Station 1 performs the following functions:

- Reads all 32 piano keys over SPI
- Supports up to four simultaneous key presses
- Converts key positions into note values
- Displays active notes on the LCD
- Transmits note information to Station 2 over CAN
- Receives tempo and metronome updates from Station 2
- Generates a local metronome using PWM
- Uses timer interrupts for metronome timing

## Keyboard Interface

The 32-key keyboard is connected through **four shift registers**, allowing the state of every key to be read through the microcontroller's SPI peripheral.

```text
32 Piano Keys
      │
      ▼
4 × Shift Registers
      │
     SPI
      │
      ▼
   Station 1
```

Each keyboard scan reads four 8-bit values for a total of 32 key states.

The software searches the 32 keys and retains the first four detected key presses. Each key position is then mapped to its corresponding musical note and prepared for CAN transmission.

## Four-Note Polyphony

Station 1 maintains four active note channels:

```text
Channel 1 ─┐
Channel 2 ─┤
Channel 3 ─┼──► CAN ──► Station 2
Channel 4 ─┘
```

If more than four keys are pressed simultaneously, only the first four detected keys are used.

Each active key is encoded as a value from 1–32 before being transmitted to Station 2.

## CAN Communication

Station 1 communicates with Station 2 using the MC9S12's MSCAN peripheral.

### Note Transmission

Station 1 transmits the four active key values using a CAN message identified by `N`.

```text
Station 1
    │
    │  Note Data
    ▼
Station 2
```

The four key values are placed into the CAN data payload and transmitted continuously as the keyboard is scanned.

### Tempo Reception

Station 1 also receives CAN messages from Station 2 containing tempo and metronome information.

```text
Station 1
    ▲
    │  Tempo / Metronome
    │
Station 2
```

The received tempo value updates the BPM used by Station 1's metronome.

## LCD Display

The LCD displays two pieces of information:

- Currently active notes
- Current tempo

Up to four notes can be displayed simultaneously. Each note is represented by its letter, accidental when applicable, and octave/number designation.

The LCD interface uses the course-provided LCD driver included with the project.

## Metronome

Station 1 generates a metronome using the MC9S12's **Enhanced Capture Timer (ECT)** and a PWM output.

The ECT generates periodic interrupts based on the current BPM. The interrupt service routine updates the PWM output to create the metronome pulse.

The tempo is controlled by Station 2 and communicated to Station 1 over CAN.

## Main Program Flow

```text
Initialize
    │
    ├── Clock
    ├── SPI
    ├── LCD
    ├── PWM
    ├── ECT
    └── CAN
    │
    ▼
Read 32 Piano Keys
    │
    ▼
Identify Up to 4 Notes
    │
    ├──────────────► Update LCD
    │
    ▼
Prepare CAN Note Data
    │
    ▼
Transmit to Station 2
    │
    ▼
Check for Tempo Update
    │
    ▼
Repeat
```

The main keyboard loop updates approximately every 5 ms.

## Peripherals Used

| Peripheral | Purpose |
| --- | --- |
| SPI0 | Read the four keyboard shift registers |
| CAN0 / MSCAN | Communicate with Station 2 |
| PWM | Generate the metronome tone |
| ECT | Generate interrupt-driven metronome timing |
| LCD / Port K | Display active notes and tempo |

## Files

```text
station_1/
├── README.md
├── main.c
├── isr_vectors.c
├── my_vectors.h
├── LCD_driver.c
├── LCD_header.h
└── derivative.h
```

### `main.c`

Contains the primary Station 1 application, including keyboard scanning, note conversion, CAN communication, LCD output, PWM configuration, and metronome timing.

### `isr_vectors.c` / `my_vectors.h`

Provide the interrupt-vector configuration used by the application.

### `LCD_driver.c` / `LCD_header.h`

Provide the interface to the Dragon12-Light LCD.

### `derivative.h`

Provides the device-specific definitions for the MC9S12DG256.

## Course-Provided Code

The LCD driver and interrupt-vector infrastructure were provided by **Dr. Andrew Jones** for EGEE 355 at Lake Superior State University and are included with permission.

Original attribution comments have been retained in the corresponding source files.
