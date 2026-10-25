----------------------------------------
PROMPT
----------------------------------------


I want to build a desk robot with my esp32 devices . I want to build one that emulates BMO from the TV show "adventure time" I want these to have a microphone input, speaker output, and a mini lcd display face. 

There would be at least 2 esp32s involved here. one to handle the display and  the face display engine, as well as any physical input buttons. 
another to handle the speaker and microphone and key word processing, perhaps a 3rd if splitting out the wake word processing to another processor would be beneficial (although it probably makes more sense to run this on the esp32 with the speaker and microphones. 

The esp32s and rasberry pi should communicate directly and not through wifi. the raspberry pi 4 would be optionally connected to wifi and would connect to a specified IP address with the ollama instance on the network (i.e. the macbook pro m1). ideally this would have the ability to spin up sub agents for asyncronous task processing. 

I want to be able to talk to this device and have it respond to me conversationally after a wake word. This will partially emulate the functionality of a rubber duck for a developer, allowing me to think through problems as well as just have a personality on my desk that resembles BMO. 

I want it to also respond from the server with notifications, such as telling me about the time, calendar events, etc. this would be sent from the rasberry pi, and the rasbperry pi could have the ability to add webhooks or something similar. 

I have a raspberry pi 4 I could use as a server to process the more complex audio input and output during conversation mode. I could also use ollama on my macbook pro m1 for more advanced queries. I do not want this to access cloud models or the internet at all directly, instead requests to ollama could optionally make requests that involve internet access. 

How can I make this system rely primarily on the raspberry pi 4 and esp32, and only query the Mac if it wants a more complex response , or if it needs access to an internet search, or anything else like that. I want this system to work even if the Mac is unavailable

describe to me specifically what needs to be set up on the esp32 and the raspberry pi 4. assume they are connected to the same wifi network.

create a step by step plan that goes from mvp with just one esp32 to handle the display, facial expressions, then add a second esp32 to handle audio input and output, then add a wake word system, then add the raspberry pi 4 and get that working, then add a connection to ollama for more advanced queries.

I already possess all the hardware I need. Here's some of the more relevant  components I already have:


* misc speakers of various sizes
* wire, solder, 3d printer, filament, etc. 
* x10 uxcell Electret Microphone (https://www.amazon.com/dp/B00FGV867M)
* x1 Waveshare ESP32-C6 1.47inch Display Dev Board, 172×320, 262K Color, 160MHz Running Frequency Single-core (https://www.amazon.com/dp/B0F48GJD19)
* x3 ELEGOO 3PCS ESP-32 Dev Boards, ESP-WROOM-32 (https://www.amazon.com/dp/B0D8T53CQ5)
* x3 ESP32-C3 Super Mini Development Board with 0.42 inch OLED (https://www.amazon.com/dp/B0F37JWCDW)
* x3 ESP32-C3 Mini (https://www.amazon.com/dp/B0D5XYBVKY)
* x6 RP2040-Zero Board, Dual-Core Cortex M0+ (https://www.amazon.com/dp/B0DXL12W59)
* x10 INMP441 MEMS Omnidirectional Microphone Module - I2S Digital Interface (https://www.amazon.com/dp/B0F94631LZ)
* x5 VKLSVAN B0DPW56J2C High Sensitivity Sound Microphone Sensor (https://www.amazon.com/dp/B0DPW56J2C)
* DROK 5W+5W Mini Audio Amplifier Board PAM8406 DC 5V Digital Stereo Power (https://www.amazon.com/dp/B077MKQJW2)
* lm352 / lm358 op amps
* many static electronic components such as resistors, capacitors, diodes, and other passive components.


















----------------------------------------
RESPONSE
----------------------------------------

# Building BMO: a local-first ESP32 + Raspberry Pi desk robot

I would build this as a small, modular robotics project: a physical BMO-inspired character that can make expressive faces, listen for your voice, talk back, act as a developer's rubber duck, and receive useful notifications from your own services.

The most important architectural decision is to make the Raspberry Pi 4 the robot's always-available local brain, the ESP32s responsible for real-time hardware, and your M1 MacBook Pro an optional high-capability brain. That gives you a robot that still works when your Mac is asleep or unavailable.

[Nuevo de nes de BMO, adventure time characters,  png | PNGEgg](https://images.openai.com/static-rsc-4/yY0eubvYNB-Y1x2RRd3caTvFwjs2pKB11dKnJN15_QWl9qoB9Q7SKJ3Jnh4smA2dLG8wnll98qYQRhP4uZJc7qUZWoQQTGUbWag-25LrTatu5zMpia_l18IkOEwk30dFf77XKCnmmRS9A1UFUqFHvflSaFdtP0CK4mVvETe1BAM?purpose=inline)

[Your Next Desk Toy Should Be a Hackable Robot - Hackster.io](https://images.openai.com/static-rsc-4/Xrhcufopb9Sg0xgA3GMNeY7rDhbMPubh3H0HvO_ZLKfrloNY54E_DT_Grwzm73piG4irlrQwvSsW4QOx9FL-xu-66NDs5v4uAUrOTdKb0zsxtk2AAr7_-9LMbxXbGmQzApcu7byF2-h13Ey9mg_EdBHiptQQvenL9hBMHPeuGXI?purpose=inline)

[歩くM5Stackちゃん（Stack-chanWalker） | ProtoPedia](https://images.openai.com/static-rsc-4/WCEWlVuaaGeWCVheTkUehfa2bo69eXDWDAc_ZygE2cWx1AcZ0tz-uyQd6jjFJf_Nt_r3cos5IObxZHd-Qhg8NjH1LZoy_nu_zdDxHL97oRN-oCu0NeBhTVldB9i8zLm5jpR2CVUixAYAhBCDDuUOElXi3SN3L94FGYKEFhiXkng?purpose=inline)

7

Your existing hardware is a good starting point. In particular, your Waveshare ESP32-C6 display board, ESP32-WROOM boards, INMP441 digital microphones, PAM8406 amplifier, and Raspberry Pi 4 give you enough to prototype the system without buying a dedicated voice-assistant kit.

One important distinction: the robot should not require an LLM to do basic things. It should be able to animate its face, respond to buttons, tell you the time, play local sounds, and handle simple conversational commands even if the Mac is offline. The Pi can provide richer local conversation, while Ollama on the Mac becomes an optional upgrade.

## 1. The architecture I recommend

## ESP32 #1 — Face and controls

Waveshare ESP32-C6 display board

Animated eyes, mouth, expressions, buttons, status indicators

Direct wired serial connection

## ESP32 #2 — Audio peripheral

ESP32-WROOM-32

INMP441 microphone, audio playback, audio buffering and wake-word detection

Direct wired USB serial or UART

## Raspberry Pi 4 — Always-on local brain

Python services, conversation state, local speech processing, notifications, webhooks

Works offline

Local-first

Optional LAN connection

## MacBook Pro M1 — Optional advanced brain

Ollama models, complex reasoning, optional web research, asynchronous tasks

The two ESP32s and the Pi should communicate over wires, not Wi-Fi. Your existing Wi-Fi network is used only when the Pi needs to reach Ollama or another explicitly permitted network service.

This diagram is the logical design, not a requirement to build everything at once. Start with just the display ESP32 and add each subsystem only after the previous one works.

### What each processor should own

| Component         | Responsibilities                                                                     | Must work without Mac? |
| ----------------- | ------------------------------------------------------------------------------------ | ---------------------- |
| Display ESP32-C6  | Face rendering, expressions, buttons, local status                                   | Yes                    |
| Audio ESP32-WROOM | Microphone capture, audio output, wake-word detection                                | Yes                    |
| Raspberry Pi 4    | Speech recognition, local responses, conversation control, notifications, task queue | Yes                    |
| MacBook Pro M1    | Optional complex LLM responses and explicitly permitted external research            | No                     |

I would not add a third ESP32 initially. Wake-word detection belongs on the audio ESP32 unless testing shows that your chosen implementation consumes too much CPU or interferes with audio capture.

## 2. Decide the software stack before wiring everything together

I would keep the firmware relatively small and put most application logic in Python on the Pi. This is easier to debug and lets you change speech engines, LLMs, notification sources, and personality behavior without reflashing the ESP32s.

[ESP32-C6 1.47" Touch Display Dev Board with IMU and SD Slot - The Pi Hut](https://images.openai.com/static-rsc-4/q9pEt3RHcQRnkoYl6sbhW2vaVP6iAOgyUnRhW9HdcNtWkDE3aaYW-RAVbuShTRqzkMpk9_Ios24hAqlQuKsso99Z1nEEcd6P-b3BEvAI_qqPGSYE8AN2uFqfaam_4m4mP4sTxHlWFyL7fuOOFoKAHHVClALiKLY1rtGrgiwHql0?purpose=inline)

Display firmware

ESP32-C6

- Arduino framework or ESP-IDF
- LVGL for graphics and animation
- A small serial protocol for face commands
- A nonblocking animation state machine

Use the official [Waveshare board documentation](https://docs.waveshare.com/ESP32-C6-Touch-LCD-1.47?utm_source=chatgpt.com) to initialize the actual screen and touch controller. The board has a 172 × 320 display, 8 MB flash on the touch variant, and an onboard IMU.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://docs.waveshare.com\&sz=32)

WaveShare Documentation



[ESP32 Scheda di Sviluppo ESP32-WROOM-32 con WiFi e Bluetooth 4MB Flash 38 pin - Vendita Online su ZioTester](https://images.openai.com/static-rsc-4/kv9q5rysf6eNeV2hIlWK5TZMiiPXmuOWqyif3TNo2xm7_tepzmsvpunNs5OWnIjBeklpSZVjT6TvOq5UI81-KV_obINniJz_xCM1quvnlE63LfFiZrKLZqWcqQt8gVOeqBvJL9Fl_d-rl-DIGlmPogTq9FZKmSMS3fwq-kqhZes?purpose=inline)

Audio firmware

ESP32-WROOM-32

- ESP-IDF with I2S audio capture and playback
- INMP441 digital microphone
- Voice activity detection and wake-word engine
- Small ring buffers and serial audio transport

Start with basic I2S capture and playback before integrating recognition. Espressif's [ESP-SR framework](https://github.com/espressif/esp-sr?utm_source=chatgpt.com) includes embedded speech-recognition components, including WakeNet.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://github.com\&sz=32)

espressif/esp-sr: Speech recognition · GitHub

+1



[Raspberry Pi 4 Model B with 8GB Ram  | Buy Electronic Components Online Pune - Rajiv Electronics](https://images.openai.com/static-rsc-4/cn-3kUX54VQeYqnlUD5jCozm_oMcdDTrUEiWve2nWELu-vMkyedFs2asATdWbaggtTuNTk2xrGrWyrE_kB_kBTd27MtN-BgAKNNE4mfF_v0OPChAwVrVV8sMSHDLr4QkUmezga5CcYNrpNgU4iPg5btJDmhSIiYtIAP8hGXL52U?purpose=inline)

Robot server

Raspberry Pi OS Lite

- Python 3 and `asyncio`
- FastAPI for local APIs and webhooks
- SQLite for conversations, settings, and queued tasks
- `whisper.cpp` for offline speech recognition
- Piper or another locally installed TTS engine
- A local rules/intent handler that doesn't require an LLM

Keep this service running independently of Ollama. Speech recognition and synthesis models must be installed locally in advance.

[Refurbished 13.3-inch MacBook Pro Apple M1 Chip with 8‑Core CPU and 8‑Core GPU - Space Grey - Apple (IE)](https://images.openai.com/static-rsc-4/o2kie_cgWUkA0vx0uqigvCfyTKy1Hp7luBPyWrHL58Iqx84O6u2--3QyJsXSs7oHKjtffFNQwYO81tfHU4OkB0EgssKA-Sjwq32jOd3JSd46qu8uB6HDXz3d_4Vj5e3C-dZ0jHuG7u65qHUrMDpuw27_Nec4Z9U8cTo5Lh2qj3U?purpose=inline)

Optional advanced inference

MacBook Pro M1

- Ollama API over your LAN
- More capable local LLMs
- Explicitly enabled web research tools
- Background task workers and summaries

The Pi should send only selected requests to the Mac. If it fails to respond within a timeout, the Pi returns to local handling.

### A note about your specific ESP32 boards

Your Waveshare display board is an ESP32-C6, not an ESP32-S3. That is fine for the display, and current versions of Espressif's ESP-SR support WakeNet on the C6 as well. The audio board I would choose first is the classic ESP32-WROOM-32, which is a good candidate for the microphone and audio I/O. Verify the exact ESP-SR release, supported model, and memory requirements before choosing the final wake-word implementation.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://github.com\&sz=32)

espressif/esp-sr: Speech recognition · GitHub

+1



The ESP32-C3 Super Mini boards are useful for future simple peripherals, but I wouldn't introduce them into the initial audio pipeline.

## 3. Plan the physical audio hardware

You already have the important components. There is one gap to account for: the INMP441 is a digital microphone, but your PAM8406 is an analog-input amplifier. They don't connect directly to one another as a complete audio path.

### Microphone input

INMP441 MEMS microphone

ESP32-WROOM-32 I2S receiver

PCM audio buffer → serial link → Raspberry Pi

### Speaker output

Raspberry Pi-generated speech audio

ESP32 I2S transmitter → I2S DAC

PAM8406 amplifier → speaker

### Microphone wiring

For the INMP441, connect the microphone to the audio ESP32's I2S interface:

| INMP441 pin | Connection                                     |
| ----------- | ---------------------------------------------- |
| VDD         | 3.3 V                                          |
| GND         | GND                                            |
| SCK / BCLK  | Selected ESP32 I2S bit-clock GPIO              |
| WS / L/RCLK | Selected ESP32 I2S word-select GPIO            |
| SD          | Selected ESP32 I2S data-input GPIO             |
| L/R         | GND or 3.3 V, according to the desired channel |

Use the pinout for your exact microphone module. Leave unused pins unconnected unless its documentation says otherwise. Keep the I2S wires reasonably short, and use a common ground.

### Speaker wiring

Your PAM8406 board can amplify an analog audio signal to drive a suitable speaker. To produce good-quality digital audio, use an external I2S DAC before the PAM8406. Alternatively, an I2S amplifier module can replace both components if you decide to simplify the design later.

Do not connect a speaker directly to an ESP32 GPIO. Also, PAM8406 speaker outputs are typically bridge-tied outputs, so don't connect a speaker terminal to ground unless the particular board explicitly specifies that arrangement.

For the first audio prototype, use one speaker and one microphone. Once the audio path works, add a mute mechanism or half-duplex conversation mode to prevent the speaker from triggering the microphone and causing feedback.

### How much audio data will the wire carry?

At 16 kHz, mono, signed 16-bit PCM:

- 16,000 samples per second
- 2 bytes per sample
- 32,000 bytes per second, or 256 kbit/s before transport overhead

That is a manageable rate for a properly configured wired serial connection. I would start with USB serial between the audio ESP32 and the Pi, then test sustained streaming and buffering before committing to the final connector and baud rate. Use sequence numbers and packet lengths so the Pi can detect dropped or malformed blocks.

For the connection between the two ESP32s, use UART with a shared ground. For the audio ESP32-to-Pi connection, USB serial is a convenient option because it avoids relying on the Pi's GPIO UART and its voltage-level considerations.

## 4. Step-by-step build plan

The following sequence is deliberately incremental. Each phase produces a working robot, rather than requiring you to finish the entire voice stack before you have something to show for it.

## Phase 1 — Build BMO's face

First MVP

Hardware: Waveshare ESP32-C6 display board only.

Goal: Make the display behave like a tiny animated character, without any networking, audio, or Pi dependencies.

1. Set up the ESP32 development environment on your Mac. Start with the vendor's example for your exact Waveshare board.
2. Draw the static face: two eyes, a mouth, and a simple teal background.
3. Add several expression states: `idle`, `happy`, `thinking`, `listening`, `speaking`, `confused`, and `sleeping`.
4. Implement animation with a nonblocking update loop. Eye blinks, pupil movement, and mouth animation should not freeze while waiting for input.
5. Map the onboard touch input or buttons to actions: cycle expressions, trigger a blink, and toggle a talking animation.
6. Create a simple debug mode that cycles through all expressions automatically.

Definition of done: BMO can animate continuously for an hour without crashing or visibly stuttering.

Deliverable: A standalone face engine with no dependencies on the other hardware.

## Phase 2 — Add the second ESP32 and wired control

Hardware: Add one ESP32-WROOM-32.

Goal: Separate face rendering from audio hardware and establish a reliable inter-processor protocol.

1. Connect the two ESP32s using UART TX/RX and a shared ground. Cross the TX and RX wires.
2. Write a small UART command parser on the display board.
3. Write a matching command sender on the audio board. For now, the audio board can send simulated events.
4. Add commands such as `FACE LISTENING`, `FACE HAPPY`, `FACE THINKING`, and `FACE IDLE`.
5. Add acknowledgements, a heartbeat, and a timeout that returns the face to a safe idle state if the other processor disconnects.
6. Test malformed messages, repeated commands, and a reset of either ESP32 while the other remains powered.

Definition of done: You can press a button on one board and reliably change the face on the other.

Deliverable: Two independently running microcontrollers communicating over a wired serial protocol.

## Phase 3 — Make the robot hear and speak

Hardware: Add the INMP441, DAC, PAM8406, and speaker.

Goal: Reliable audio capture and playback, without wake-word detection yet.

1. Configure I2S on the WROOM board and record short PCM buffers from the microphone.
2. Validate the samples on your development computer. Check for clipping, silence, channel issues, and noise.
3. Implement speaker playback. Start with a known test tone or short prerecorded PCM sample.
4. Connect the I2S DAC to the PAM8406 and verify clean output at low volume.
5. Add a small audio ring buffer, and ensure recording doesn't block the face-control task.
6. Add half-duplex behavior: pause microphone submission during playback, then resume listening when playback ends.
7. Test longer sessions and repeated playback without resets or buffer overruns.

Definition of done: BMO can record your voice and play a known sound reliably.

Deliverable: Working audio hardware and reusable capture/playback code.

## Phase 4 — Add wake-word detection

Goal: Let BMO notice when you're addressing it, without continuously sending audio to another computer.

1. Run a lightweight wake-word engine on the audio ESP32.
2. Begin with a supported built-in wake phrase to validate the pipeline. Later, investigate a custom BMO-style phrase.
3. Keep the microphone sampling continuously and feed the recognizer in the format its model requires.
4. When the wake word is detected, signal the display board to enter `listening` mode.
5. Capture the full utterance after the wake word until voice activity detection detects the end of speech.
6. Send a `SPEECH_START` event to the Pi-facing transport layer you will implement in Phase 5.
7. Add a physical mute control and a visual listening indicator.

Definition of done: BMO recognizes the wake phrase repeatedly in ordinary desk conditions and doesn't respond constantly to television or background noise.

Deliverable: An always-listening wake-word subsystem that runs locally.

Implementation note: Use Espressif's [ESP-SR documentation](https://github.com/espressif/esp-sr?utm_source=chatgpt.com) as the starting point. WakeNet supports the classic ESP32 and ESP32-C6 in documented releases, but custom wake phrases and model compatibility need to be verified against the selected release.&#x20;

[image](https://www.google.com/s2/favicons?domain=https://github.com\&sz=32)

espressif/esp-sr: Speech recognition · GitHub

+1



## Phase 5 — Introduce the Raspberry Pi 4

Goal: Make BMO conversational using entirely local processing.

1. Install Raspberry Pi OS Lite and enable SSH for setup.
2. Connect the audio ESP32 to the Pi via USB serial.
3. Implement a Python serial receiver that reconstructs PCM audio from framed packets.
4. Install an offline speech recognizer and a local TTS engine, and download their model files in advance.
5. Build a Python conversation service that accepts an audio utterance, transcribes it, determines the intent, and produces a response.
6. For the first version, use deterministic handlers for commands and simple conversational templates. Do not wait for an LLM to tell you the time.
7. Generate speech with local TTS, stream the audio back to the ESP32, and play it through the speaker.
8. Send `THINKING`, `SPEAKING`, and `IDLE` events to the display controller.
9. Add time announcements and a small queue for scheduled notifications.

Definition of done: You can say a wake phrase, ask a simple question, and hear a spoken response while the MacBook is offline.

Deliverable: A fully local voice-assistant loop, with the Pi coordinating the interaction.

## Phase 6 — Add Ollama as an optional advanced brain

Goal: Let BMO handle more complex reasoning without making the Mac a required service.

1. Install and configure Ollama on the MacBook Pro M1.
2. Configure the Pi with the Mac's LAN IP address and Ollama port, typically `11434`.
3. Implement a local model router on the Pi that classifies each request as a simple local task or an advanced query.
4. Send only eligible advanced queries to the Mac.
5. Set a connection timeout, request timeout, and bounded retry policy.
6. If Ollama is unavailable, fall back to a local response or explain the limitation.
7. Add an explicit permission setting for requests that may use internet search. The Pi itself must not silently access the internet.
8. Add asynchronous task workers only after the synchronous request/response loop is reliable.

Definition of done: Complex questions can use Ollama, while the robot remains functional if the Mac is shut down or disconnected.

Deliverable: A local-first assistant with optional advanced inference.

## 5. What exactly should run on the Raspberry Pi?

Once you reach Phase 5, I would organize the Pi software into separate modules rather than putting everything into one large Python script.

```
bmo/
├── app/
│   ├── main.py             # Application startup and shutdown
│   ├── config.py           # Settings, paths, and service addresses
│   ├── conversation.py     # Turn-taking and conversation state
│   ├── router.py           # Local vs. Ollama decision
│   ├── intents.py          # Deterministic local commands
│   ├── personality.py      # BMO's conversational style
│   ├── notifications.py    # Scheduled and event-driven messages
│   ├── tasks.py            # Background task queue
│   └── security.py         # Authentication and permission checks
├── hardware/
│   ├── serial_audio.py     # ESP32 audio transport
│   ├── face_protocol.py    # Face commands and status
│   └── audio_pipeline.py   # Speech recognition and TTS
├── integrations/
│   ├── ollama_client.py    # Optional Mac connection
│   ├── calendar.py         # Calendar provider adapter
│   └── webhooks.py         # Incoming local events
├── data/
│   └── bmo.sqlite3
├── models/                 # Locally installed model files
├── tests/
└── requirements.txt
```

This is a proposed layout, not a requirement to implement every module immediately. For the first Pi prototype, `main.py`, `serial_audio.py`, `audio_pipeline.py`, and `intents.py` are enough.

### Speech recognition and synthesis

The Pi 4 can do useful local speech processing, but it has limited CPU and RAM compared with your Mac. Start with small, quantized speech models and measure actual latency before selecting a larger model.

| Task                     | Suggested starting point                                  | Notes                                                   |
| ------------------------ | --------------------------------------------------------- | ------------------------------------------------------- |
| Speech-to-text           | `whisper.cpp` with a small quantized model                | Offline after installation; benchmark latency on the Pi |
| Text-to-speech           | Piper or another locally installed lightweight TTS engine | Generate audio on the Pi, then stream PCM to the ESP32  |
| Voice activity detection | WebRTC VAD or a suitable local VAD                        | Helps determine when you've finished speaking           |
| Simple commands          | Python intent handlers                                    | Fast, predictable, no LLM needed                        |
| Complex conversation     | Ollama on the Mac, when available                         | Optional and replaceable                                |

Do not assume that all speech models will be fast enough for natural conversation on the Pi 4. Benchmark a representative 5–10 second utterance, including transcription, response generation, and speech synthesis. Optimize the pipeline based on measured delays.

For your first version, you can also skip wake-word detection entirely while developing the Pi. Use a physical button to start recording, then process the captured audio. This isolates audio transport and speech processing bugs from wake-word bugs.

### How the Pi should communicate with the ESP32s

Define a small protocol early so that you can test each component independently.

For control messages, newline-delimited JSON is easy to debug:

```
{"type":"face","state":"listening"}
{"type":"face","state":"thinking"}
{"type":"face","state":"speaking"}
{"type":"face","state":"happy"}
```

Use framed binary packets for audio instead of Base64-encoding audio inside JSON. A packet header should include:

- Protocol version
- Message type
- Sequence number
- Payload length
- Optional timestamp or sample offset

Possible message types include `AUDIO_START`, `AUDIO_DATA`, `AUDIO_END`, `PLAYBACK_START`, `PLAYBACK_DATA`, `PLAYBACK_END`, `WAKE_WORD`, and `ERROR`.

The face ESP32 doesn't need to know anything about speech recognition or Ollama. It receives only the face state and any small animation parameters. That separation makes the hardware much easier to maintain.

## 6. How to make local-first conversation work

The key is to put a decision-making layer on the Pi before anything reaches Ollama.

For example, these requests should not all be treated the same way:

| Request                                              | Where it should run                             |
| ---------------------------------------------------- | ----------------------------------------------- |
| “What time is it?”                                   | Pi, using system time                           |
| “Remind me about my meeting in 10 minutes.”          | Pi, using a local scheduler                     |
| “Help me think through this function.”               | Pi first; Ollama if needed                      |
| “Summarize this long technical document.”            | Pi if feasible; otherwise Ollama                |
| “Search the web for recent changes to this library.” | Explicitly authorized Mac-side research         |
| “Ollama is offline. Can you still talk to me?”       | Pi, using local handlers and fallback responses |

I would use three routing levels:

### Level 1 — Local commands

Time, timers, status, reminders, notifications, basic controls, and simple conversation. No model required.

### Level 2 — Local intelligence

Offline speech recognition, intent classification, small local models if practical, conversation history, and task orchestration on the Pi.

### Level 3 — Optional advanced intelligence

Ollama on the Mac for difficult reasoning, longer context, code analysis, and approved external research.

### One important distinction about the Pi

The Pi 4 should be your always-on application server, but it doesn't have to run a large language model to be useful. You can build a satisfying, interactive MVP with local command handlers, prerecorded responses, offline speech recognition, and local TTS before you attempt full open-ended conversation.

A small local language model on the Pi is a possible later experiment, but I would not make it a dependency. Your architecture should still work without one.

For the rubber-duck functionality, I'd also have a specific conversational mode. When you say something like “BMO, help me think this through,” BMO could ask one question at a time, summarize your current understanding, and challenge assumptions. That behavior can be guided by a personality prompt when Ollama is available, with simpler scripted behavior as the fallback.

## 7. Ollama, network access, and asynchronous tasks

Your MacBook Pro M1 can host the more capable model service. The Pi connects to Ollama over your LAN using the Mac's IP address.

For example, your Pi configuration might eventually contain:

```
OLLAMA_BASE_URL=http://192.168.1.50:11434
OLLAMA_ENABLED=true
OLLAMA_TIMEOUT_SECONDS=30
WEB_RESEARCH_ENABLED=false
```

The IP address is only an example; use your Mac's actual LAN address. Reserve that address in your router or configure a stable local hostname.

Ollama's API is commonly available on port `11434`. Configure network access deliberately: avoid exposing the service to the public internet, and don't assume the default API provides authentication. Restrict access to your trusted LAN or use a suitable firewall and private network configuration.

### How the optional internet permission should work

I would make internet access a distinct capability rather than something that happens automatically when the model is asked a question.

1. BMO sends a request to the Pi.
2. The Pi decides whether local handling is sufficient.
3. If the request needs advanced reasoning, the Pi sends it to Ollama.
4. If it needs web research, the Mac-side service may invoke a specifically configured search tool, but only when the relevant permission is enabled.
5. The results are summarized and returned to the Pi.
6. If the Mac or the external search service is unavailable, BMO explains the limitation and continues operating locally.

This is especially important if you want the system to remain genuinely local-first. Sending a request to a model on your LAN does not itself imply internet access, but a model equipped with web tools can cause external requests. Keep those two capabilities separate.

### Asynchronous tasks and subagents

You can implement background task processing without requiring the ESP32s to do any of the heavy work.

For instance, BMO could respond to “Look into this library and tell me what you find later” by acknowledging the request, adding a job to SQLite, and continuing the conversation. A background worker can process the job and queue a notification for later.

I'd use this pattern:

- Pi: Owns the task queue, task status, schedules, and notifications.
- Mac: Optionally executes complex reasoning tasks through Ollama.
- Workers: Run independent jobs with timeouts, status updates, and stored results.
- BMO: Announces completion when a task is ready.

You do not need multiple LLM agents to achieve this. Start with a queue and one worker. Later, you can add specialized workers for coding help, document analysis, or web research. A true multi-agent workflow requires orchestration code; Ollama alone does not automatically create a team of autonomous agents.

## 8. Notifications, calendar events, and webhooks

The Pi is the natural place to implement notifications because it can remain active even when the Mac is unavailable.

A useful first set of features would be:

- Announce the time on request.
- Speak reminders at scheduled times.
- Notify you when a background task finishes.
- Accept an event from another local application through a webhook.
- Display a notification icon or expression on BMO's face.
- Queue notifications while BMO is speaking, so they don't interrupt a conversation.

For a calendar integration, choose a provider adapter appropriate to your calendar. If you want to avoid cloud access, use a locally accessible calendar source or a locally synchronized calendar file. If the calendar itself is hosted by a cloud provider, keeping the robot's speech processing local does not eliminate that provider's network access.

A webhook could look conceptually like this:

```
POST /api/notifications
Content-Type: application/json
```

```
{
  "source": "kanboard",
  "title": "Task completed",
  "message": "The project build finished.",
  "priority": "normal"
}
```

The Pi validates the request, stores the notification, and sends an event to the face controller. If it is appropriate to speak the message, the notification enters the audio queue.

For security, keep webhook endpoints on the local network, require an authentication token, validate input, and avoid exposing an unauthenticated notification endpoint. Also distinguish between trusted local integrations and any service that is allowed to trigger external research.

## 9. A practical build checklist

Use this as the master list for the project. Complete the early milestones before adding the more complicated ones.

## Project progress

### 0/31

### Phase 1: Animated face

Initialize the Waveshare display

Implement idle, blink, happy, thinking, listening, and speaking states

Add button or touch input

Run a long-duration animation test

### Phase 2: Wired controller link

Connect the ESP32s by UART

Implement command framing and parsing

Add heartbeat and disconnect recovery

### Phase 3: Audio hardware

Capture audio from the INMP441

Verify sample quality

Build the I2S DAC and amplifier playback path

Add buffers and playback muting

### Phase 4: Wake word

Run a supported local wake-word model

Detect utterance start and end

Drive the face listening state

Add physical microphone mute

### Phase 5: Raspberry Pi voice server

Install Raspberry Pi OS Lite and Python

Stream framed audio over USB serial

Install offline STT and TTS models

Implement local intents and spoken replies

### Phase 6: Notifications

Add local time and timer functions

Create a SQLite notification queue

Implement authenticated webhooks

Test notification delivery while offline

### Phase 7: Optional Ollama

Configure Ollama on the Mac

Add the Pi-side request router and timeouts

Implement offline fallback

Add explicit web-research permissions

### Phase 8: Polish and enclosure

Tune microphone placement and speaker volume

Print the BMO-inspired enclosure

Test thermals, cable strain relief, and power stability

Run an extended reliability test

Reset checklistCopy checklist

## 10. What I would do first, with the hardware you own

I'd begin with only the Waveshare ESP32-C6 and your Mac development environment.

Make the display look like BMO, build a handful of expressions, and create a small command interface that lets you change the face. Don't begin with the Raspberry Pi, speech recognition, or Ollama. Those components will be much easier to debug when you already have a working display engine and a clear protocol.

Next, bring up the ESP32-WROOM and INMP441 independently, then get the speaker path working. Once those components are reliable, add the Pi and the local speech pipeline. The Mac integration should be the final major subsystem, not a prerequisite for the first conversational prototype.

One design principle worth preserving throughout the project: the face should be responsive even when the brain is busy. Keep display animation, audio capture, and network requests from blocking one another. That will make BMO feel much more like a character and much less like a computer with a screen attached.
