# Garage Gadget

ESP32-S3 garage door monitor and controller.

Knows whether your garage door is open, can trigger it, closes it for you if
you forget, and tells you about it — on a dashboard it serves itself, and on
your phone.

**One firmware image serves every unit.** Nothing is compiled in: the owner
configures Wi-Fi, credentials, dashboards and alerts through a setup wizard
on first power-up, and everything is stored in NVS. No per-customer build,
and no credentials anywhere in the source tree.

---

## Quick start

**1. Power it on.** The light on the board tells you what it is doing.

| Light | Meaning | What to do |
|---|---|---|
| 🔵 **Blue**, slowly breathing | Waiting to be set up | Go to step 2 |
| 🟢 **Green**, steady | Working. Door closed | Nothing |
| 🟡 **Amber / red**, flickering | Door is open | Nothing |
| 🔴 **Red**, fast flashing | Alarm | Check the door |
| 🟣 **Magenta**, slow flashing | Lost Wi-Fi | It keeps retrying by itself |
| ⚪ **White**, steady | Starting up | Wait a few seconds |

**2. Set it up.** On your phone, join the Wi-Fi network **GarageGadget-XXXX**,
password **garagesetup**. The setup page usually opens by itself; if not, open
**http://192.168.4.1**. Work through the five steps, then rejoin your normal
Wi-Fi and open the address shown on the last screen.

**3. Let it move the door.** A new unit will not open your door, on purpose.
Switch **Door control** on at the top of the **Control** tab once the unit is
mounted and the door state on screen matches the real door.

**Forgot the password?** Hold the recovery button for 10 seconds while the
unit is running. Nothing else is erased.

> A printable version of this card ships with each unit — see
> [QUICKSTART.md](QUICKSTART.md).

---

## Contents

1. [What it does](#1-what-it-does)
2. [Hardware](#2-hardware)
3. [First-time setup](#3-first-time-setup)
4. [Using the dashboard](#4-using-the-dashboard)
5. [Notifications](#5-notifications)
6. [Where your data lives](#6-where-your-data-lives)
7. [Security](#7-security)
8. [When something goes wrong](#8-when-something-goes-wrong)
9. [Building and flashing](#9-building-and-flashing)
10. [How the firmware is organised](#10-how-the-firmware-is-organised)

See also: **[homeassistant/](homeassistant/)** for Home Assistant,
**[TELEGRAM.md](TELEGRAM.md)** for notification setup, and
**[grafana/](grafana/)** for the optional dashboard and alert templates.

---

## 1. What it does

- **Door position sensing** from a reed or hall sensor, read as an
  analogue voltage with a user-settable threshold, so it works with
  whatever sensor and wiring the installation happens to have.
- **Door control** — a relay pulse across the opener's manual button,
  behind a master switch that ships **off**. See [Arming door control](#arming-door-control).
- **Auto-close** — shuts the door after a configurable delay, with a hard
  lock-off switch for when you genuinely want it left open.
- **Two alarms** — *Immediate* (the door should not be opening right now)
  and *Left Open* (it has overstayed its delay, and keeps escalating).
- **Phone notifications** sent straight from the device over Telegram or
  ntfy. No broker, no cloud account of ours, nothing to host.
- **Its own dashboard and history** — the last 10 days of charts and a
  long-running event log, served from the device, with no external
  database required.
- **Home Assistant** over MQTT, with auto-discovery: HA builds the garage
  door, sensors and switches by itself. See [`homeassistant/`](homeassistant/).
- **Optional InfluxDB + Grafana** for owners who already run them. See
  [`grafana/`](grafana/).
- **OTA firmware update** by dragging a `.bin` onto the Update tab, with an
  optional daily check that tells you when a new release exists.

---

## 2. Hardware

Built for **ESP32-S3** with **16 MB flash** and **8 MB octal PSRAM**
(an `-R8` part), on ESP-IDF **5.3**.

| Function | GPIO | Direction | Notes |
|---|---:|---|---|
| Door position sensor | 6 | **analogue in** (ADC1 ch5) | ≥ threshold = closed. See below. |
| Relay control | 18 | pulse output, active low | idles as a high-Z input for safety |
| RGB status LED (SK6812) | 48 | output | one LED, RMT driven |

The relay pin sits as an **input** when idle, so a reset or a crash cannot
leave the opener's button held down. It is switched to a low output for a
brief pulse and immediately released.

**Pulse length** defaults to 100 ms and is adjustable in **Settings → Configuration → Device → Relay
pulse length** (20–2000 ms). Raise it if your opener sometimes ignores the
press. Do not raise it far: many openers treat a multi-second hold as a
request to enter programming or limit-setting mode, which is why the upper
bound exists.

### Door sensor — analogue, not digital

The sensor pin is read as a **voltage**, not a logic level. Reed switches and
hall sensors wired through a pull-up or a divider often do not swing all the
way to the rails, and a plain digital read then either works by luck or not
at all. Reading millivolts means one firmware covers every wiring variant and
the trip point becomes a setting rather than a property of the hardware.

- **At or above the threshold → CLOSED.** Below it → OPEN.
- Default threshold **1800 mV**, adjustable in **Settings → Configuration → Door sensor → Door-closed
  threshold**.
- A **±150 mV dead-band** around the threshold stops a sensor resting near
  the line from chattering and filling the event log with phantom opens.
- Each reading is the **median of 5 samples**, so one wild ADC sample cannot
  flip the state.
- ADC1 is used deliberately: ADC2 shares hardware with the Wi-Fi radio and
  its reads fail whenever Wi-Fi is active, which here is always.

### Calibration

**Settings → Configuration → Door sensor.** Rather than asking you to work out a
threshold, the device measures what your sensor actually does:

1. **Close the door** fully, let it settle, press **Capture closed**.
2. **Open the door**, let it settle, press **Capture open**.

Each capture averages 64 readings over about a second and reports how much
the value wandered while measuring. The device then classifies by **nearest
reference**: whichever captured reading the live value is closer to wins. The
trip point is simply the midpoint of the two.

Because it works on "nearest", **polarity does not matter** — a sensor that
reads *lower* when the door is shut works exactly as well as one that reads
higher. You do not have to tell it which way round your sensor is.

The dead-band is derived from the measured separation (a quarter of it,
clamped to 30–300 mV) instead of being a fixed number, so a sensor with a
small swing gets a proportionally tighter band.

The panel shows a live position strip: your two captured points, the trip
point between them, and a needle for the current reading. If the needle sits
firmly in the green half when the door is shut and the red half when it is
open, you are done.

**If calibration is refused**, the two readings were less than 200 mV apart.
That means the sensor is not actually distinguishing the two positions —
usually a floating input, a missing pull-up, or too large an air gap. The
device keeps using the manual trip point until you fix it and re-capture.

**Manual alternative.** You can still type a **Trip point** directly and skip
calibration; in that case higher means closed and the dead-band is the fixed
`APP_DOOR_HYSTERESIS_MV`.

The Control tab shows the live reading at all times as
`1882 mV · raw 2232 · trip 1800`, right above the chip temperature.

If the value reads `(uncal.)`, the chip had no ADC calibration data in eFuse
and the voltage is a linear estimate — accurate to a few percent, which is
still fine for a threshold comparison.

### Status LED

| Colour | Meaning |
|---|---|
| dim white | booting |
| blue, breathing | first-time setup — join the setup Wi-Fi |
| magenta, slow blink | configured, but cannot reach Wi-Fi |
| dim green | closed, online, nothing to report |
| amber/red, fast blink | door open |
| red, fast blink | alarm |
| brief blue flash | the relay just fired |

### Other boards

Change the GPIO constants at the top of `main/magnet.cpp`, `main/relay.c`
and `main/led_rgb.c`. If you move the door sensor, it must land on an **ADC1**
pin (GPIO1–GPIO10 on the S3); update both `MAGNET_ADC_CHANNEL` and the comment
naming the GPIO. If your board has quad PSRAM or none, adjust
`sdkconfig.defaults` — the comments there say exactly what to change. With
no PSRAM the firmware still runs and automatically drops from 10 days of
chart history to 2.

---

## 3. First-time setup

No cables, no config files.

1. **Power the unit.** The status LED breathes blue.
2. **On your phone, join the Wi-Fi network `GarageGadget-XXXX`.**
   The password is `garagesetup`. `XXXX` is from the device's MAC, so
   several units on one bench stay distinguishable.
3. **Open `http://192.168.4.1`.** Most phones offer this automatically as a
   "sign in to network" notification.
4. **Work through the five steps:**

   | Step | Required? | |
   |---|---|---|
   | 1. Wi-Fi | **yes** | pick your network from the scan list, or type it |
   | 2. Device name | **yes** | becomes `http://<name>.local` and tags your data |
   | 3. InfluxDB / Grafana | no | skip unless you already run them |
   | 4. Notifications | no | Telegram and/or ntfy — see [§5](#5-notifications) |
   | 5. Admin password | **yes** | 4–32 characters |

5. **Finish.** The device joins your Wi-Fi. Reconnect your phone to your
   normal network and open `http://<device-name>.local`.

The door cannot be moved during any of this — see
[Arming door control](#arming-door-control).

If the Wi-Fi details were wrong the device keeps its setup network up until
setup actually completes, so you can correct them. You cannot lock yourself
out.

---

## 4. Using the dashboard

`http://<device-name>.local` (or the IP shown in the header).

### Arming door control

**A new device will not move the door.** The relay master switch on the
**Control** tab defaults to off, and until you turn it on the relay is never
energised — not by the button on this page, not by auto-close, not by Home
Assistant.

That default is deliberate. A unit on the bench is often already wired to a
real opener, and before calibration the sensor reads whatever a floating pin
feels like — usually "open". Auto-close is on by default. Without this
switch, the first thing a half-configured unit does is drive the door while
the installer is stood underneath it with a screwdriver.

Turn it on once the unit is mounted, the sensor is calibrated and the door
state on screen matches the real door. The setting is stored in NVS, so a
power cut cannot silently re-arm a door you switched off on purpose.

The firmware also refuses to fire while the device is still being set up, or
while it is showing its setup hotspot, whatever the switch says. A unit being
configured is a unit someone is standing next to.

One case is deliberately **not** blocked: *rescue* mode, where a configured
device raised its hotspot because the router went away. That unit is fully
set up, and refusing to shut someone's garage because their Wi-Fi is down
would be its own kind of failure.


A sidebar on the left holds the four sections; on a phone it becomes a strip
along the top. It also shows the device name and the live door state, so the
answer to "is my garage shut" is on screen whatever you are looking at.

**Control** — no password needed. A garage graphic whose door panel slides
open and shut with the real one, the trigger button, both alarm lamps, the
auto-close settings, and device vitals: door sensor voltage and raw ADC
count, chip temperature, Wi-Fi signal, uptime.

**History** — 24 h / 3 d / 7 d / 10 d of door position, Wi-Fi signal and chip
temperature, plus the full event log. All from the device's own storage.

**Settings** — three sub-tabs:

- *Configuration* — split again by a second bar into **Wi-Fi**, **Device**,
  **Door sensor**, **Dashboards** (InfluxDB and Grafana), **Home Assistant**,
  **Notifications**, **Security** (trusted devices and the admin password) and
  **Maintenance**. One section shows at a time, but **Save all settings** stays
  pinned to the bottom of the page and writes every section at once — you do
  not need to save before switching.
- *Terminal* — the live log, the same text you would see on the serial port.
- *Firmware* — drag a `.bin` on to update over the air. The device validates
  the image before switching to it and falls back to the running firmware if
  it does not verify.

**Help** — the whole manual, on the device. Categorised, one topic per page,
and readable **without signing in**: the recovery instructions are needed
exactly when you cannot get in, and none of it is specific to your unit.
Every settings section has a **?** beside its heading that jumps straight to
the page explaining it.

The Help content is generated from the Markdown in this repo by
`tools/gen_help.py`, so it can never drift from the shipped documentation.
Re-run that script after editing any `.md` file, then rebuild.

Secrets are stripped on the way in: the generator masks anything shaped like
an API token, so a real value pasted into the docs while debugging cannot end
up published in every unit's built-in manual.

History and Settings ask for the admin password, as does anything that moves
the door. You stay signed in for 12 hours; if the device reboots in the
meantime the page simply asks again and resumes what you were doing.

> Pressing **Send test notification** blocks the UI for a few seconds while
> the device actually contacts Telegram/ntfy. That is expected — it is
> reporting a real delivery result, not a guess.

### Giving the device a fixed address

By default the device takes whatever address DHCP offers, which is fine until
something needs to point at it: a Home Assistant entry, a bookmark, a
port-forward. Two ways to pin it down:

**A reservation in your router** (preferred). Find the device's MAC in the
router's client list and reserve an address for it. Nothing is stored on the
device, and it still works if you move it to a different network.

**Settings → Configuration → Device → Fixed IP address**, if your router cannot do reservations or is
not yours to configure. Fill in the address and your gateway; the mask
defaults to `255.255.255.0` and a blank DNS means "ask the gateway". It takes
effect on the next restart.

| Field | Required | |
|---|---|---|
| Fixed IP address | — | blank means DHCP |
| Gateway | **yes, if an IP is set** | your router |
| Subnet mask | no | blank = `255.255.255.0` |
| DNS server | no | blank = same as the gateway |

The obvious danger with a hand-set address is that a wrong one still joins
the Wi-Fi perfectly and is simply unreachable, which is a bad failure to
have on a device bolted to a garage ceiling. So after the interface comes up
the device pings its gateway, and if nothing answers it abandons the fixed
address and takes a DHCP lease for that boot. Your setting is left exactly as
you typed it and a warning appears at the top of the dashboard saying what
happened. A typo costs a reboot, not a ladder.

A gateway on the wrong subnet, a duplicate address already in use, or a mask
that excludes the gateway all land in the same place: the ping fails and you
get the lease. The one case it cannot catch is an address that works but
belongs to something else, which is what reservations exist to prevent.

---

## 5. Notifications

The device posts directly to a public push endpoint over HTTPS. Set up
either or both under **Settings → Configuration → Notifications**.

### ntfy — easiest, no account

1. Install **ntfy** ([iOS](https://apps.apple.com/app/ntfy/id1625396347) /
   [Android](https://play.google.com/store/apps/details?id=io.heckel.ntfy)).
2. Pick a topic name **nobody could guess** — e.g. `garage-7f3a91c4e2`.
   Anyone who knows the topic can read your alerts, and there is no
   password on a public ntfy topic.
3. Subscribe to that topic in the app.
4. On the device: switch **ntfy** on, leave the server as `https://ntfy.sh`,
   enter the topic, **Save all settings**, then **Send test notification**.

Self-hosting ntfy works too — put your own URL in the server field.

### Telegram

**Full walkthrough, including how to manage the bot afterwards and what every
error message means: [TELEGRAM.md](TELEGRAM.md).** Short version:

1. Message **@BotFather**, send `/newbot`, follow the prompts, copy the whole
   token (`123456789:AAH…`, colon included).
2. **Open your new bot and press START.** Telegram blocks bots from messaging
   you until you message them first — this is the step people miss.
3. Message **@userinfobot** to get your numeric chat ID.
4. Sanity-check both in a browser before touching the device:
   `https://api.telegram.org/bot<TOKEN>/getMe` should return `"ok":true`.
5. On the device: switch **Telegram** on, paste the token and chat ID,
   **Save all settings**, then **Send test notification**.

### Multiple recipients

**Settings → Configuration → Notifications → Recipients.** Add as many as eight chat IDs,
each with a nickname so the list stays readable. Every recipient receives
every alert you have enabled.

Delivery is per-recipient: one bad chat ID cannot stop the others from being
told the garage is open. A partial failure reports which nickname failed and
why.

Press **Find chat IDs** to have the device ask Telegram who has messaged the
bot recently, then tap the right entry. This is the reliable way to get a
chat ID — the number before the colon in your bot token is the *bot's* id,
and using it by mistake produces a "bot can't send messages to bot" error
that explains nothing.

An existing single chat ID is migrated into the list automatically on the
first boot after updating; nothing to redo.

### Choosing what to be told about

| Toggle | Default | |
|---|---|---|
| Alarms | **on** | the ones that matter |
| Door opens | off | noisy on a normal household door |
| Door closes | off | but a close that *clears an alarm* is always sent |
| Boot and Wi-Fi recovery | **on** | so you know the device came back |

The device cannot tell you it is offline. If that matters, use the Grafana
offline rule in [`grafana/`](grafana/).

---

## 6. Where your data lives

By default, entirely on the device — two tiers, chosen to match how the data
is actually used.

| | Events | Samples |
|---|---|---|
| **What** | door opened/closed, relay pulses, alarms, boots, Wi-Fi drops | door state, Wi-Fi signal, chip temperature |
| **When** | as they happen | once a minute |
| **Where** | 1 MB flash partition | PSRAM |
| **Survives reboot** | **yes** | no |
| **Capacity** | ~65,000 records — over a year in practice | 10 days (2 days without PSRAM) |

Events go to flash because losing the record of your garage opening at 3 a.m.
is unacceptable. The per-minute sample trace is chart texture, not a record,
so it stays in RAM — which also means routine polling causes no flash wear
at all. Flash writes happen only when something genuinely occurs.

The event log is a ring buffer of 16-byte records with a CRC on each; the
head is recovered on boot by reading one record per sector, so a power cut
mid-write costs at most the record being written.

**Settings** live in NVS, separately, and are never touched by history
writes.

Both tiers are readable from the History tab, and erasable from
**Settings → Configuration → Maintenance**.

If you would rather have the data in InfluxDB and Grafana, turn it on in
Settings — the on-device history keeps working alongside it. See
[`grafana/`](grafana/).

---

## 7. Security

- **One admin password** gates the door button, all settings, history, logs
  and OTA. Set during setup; changeable under **Settings → Configuration → Security**.
- **Token-based sessions.** The password is exchanged once for a random
  128-bit bearer token sent as a header. It is never put in a URL, so it
  cannot leak through server logs, browser history or `Referer`. Changing
  the password invalidates every session immediately.
- **Secrets are write-only over the API.** The settings endpoint reports
  whether a Wi-Fi password, InfluxDB token or Telegram token is *set*, never
  what it is. Leaving a secret field blank keeps the stored value.
- **Failed logins are rate-limited** by a deliberate delay.
- **Trusted devices skip the prompt, but only until the next restart.** An
  address that authenticates once is remembered so a household phone is not
  challenged on every visit. The list lives in RAM and is never written to
  flash, so a power cycle empties it and trust has to be re-earned; changing
  the password empties it too. Manage it under **Settings → Configuration → Security**.
  Note that the trust follows the IP address, so give anything on that list a
  DHCP reservation.
- **Setup endpoints close after setup.** While the device is unprovisioned
  the wizard's endpoints are open — there is no password yet. The moment
  setup completes they require authentication like everything else, so the
  recovery page is not a way around the password.
- **The live door state is deliberately public**, because the front page has
  to render before anyone signs in. History, logs and settings are not.

This is a device on your home LAN. It speaks plain HTTP — a TLS certificate
for `garage.local` is not something an end user can reasonably obtain. Treat
LAN access as trusted, and do not port-forward it to the internet.

---

## 8. When something goes wrong

**The button does nothing, or says the door will not switch.**
Door control is off, or the device is still in setup. The **Control** tab
shows which, right under the switch. A new or factory-reset unit ships with
the relay disarmed on purpose — see
[Arming door control](#arming-door-control). The button is greyed out
whenever the firmware would refuse, so the page and the device always agree.

**The dashboard is unreachable.**
Look at the LED. Magenta means it is configured but off Wi-Fi; wait — it
retries on a backoff up to 5 minutes. Blue breathing means it has dropped
into setup mode; join `GarageGadget-XXXX` and reconfigure.

**You changed routers, or mistyped the Wi-Fi password.**
Hold the recovery button for 10 seconds. The device restarts, raises its own
network, and serves a page where you set the new Wi-Fi details (and a new
password). Join `GarageGadget-XXXX`, password `garagesetup`, and open
`http://192.168.4.1`. The hotspot shuts down again once the device is on your
network.

The device deliberately does **not** raise a hotspot on its own when it cannot
reach the stored network. A router reboot or a brief outage is usually
temporary, and an unattended access point left broadcasting for hours is a
worse outcome than a device that quietly keeps retrying. The hotspot only ever
appears during first-time setup or a button-initiated recovery.

**A link opens as `https://` and fails.**
The device speaks plain HTTP — a LAN appliance cannot obtain a certificate for
a private address. If your browser has "always use secure connections" (Chrome)
or HTTPS-Only Mode (Firefox) enabled, it silently rewrites the address and then
cannot connect, which looks like a broken device. Type the address with
`http://` in front, or add an exception for the device. Every address the
device prints, including the one in each notification, already includes the
`http://` prefix for this reason.

**`.local` doesn't resolve.** Some Android versions and Windows setups don't
do mDNS. Use the IP address — it's in the dashboard header, and in your
router's client list.

**Telegram says "bot token rejected" (HTTP 401).**
The token is not one Telegram recognises. The device tells you the bot id and
key length it is using — compare them against BotFather: send `/mybots`, pick
the bot, choose **API Token**. Copy the whole string in one go, colon
included. If in doubt, **Revoke** and use the fresh token. (A token pasted
with a stray space or line break is cleaned up automatically, so that is no
longer a cause of this.)

**Telegram says "chat not found" or "Forbidden".**
Open Telegram, find your bot, and send it any message. Telegram blocks bots
from starting a conversation, so until you message it first it cannot reply.
Then check the chat ID from @userinfobot.

**The door state is wrong, or flickers.**
Look at the live sensor voltage on the Control tab with the door shut and
again with it open, then set **Settings → Configuration → Door sensor → Door-closed threshold** between the
two. See [§2](#2-hardware).

**You forgot the admin password.**

**Hold the recovery button for 10 seconds** while the device is running. The
LED flashes white three times, the device restarts, and the dashboard is
replaced by a recovery page asking you to set a new password. You may set new
Wi-Fi credentials at the same time if the device can no longer reach your
network.

Everything else is kept: door calibration, InfluxDB, Grafana, Telegram and
ntfy recipients, and the complete event history. Only the password changes.

On a dev board the recovery button is **BOOT** (GPIO0). Note that it is
checked *while the device runs*, not during power-up — GPIO0 is a strapping
pin, and holding it low at reset puts the chip into USB download mode where
this firmware never executes. Power the device on, wait for it to boot, then
hold the button.

While a recovery is armed the device refuses the old password, every session,
and every trusted device, and serves nothing but the recovery page. A
half-finished recovery therefore cannot leave the unit unprotected.

*Last resort, if the button is unreachable:* erase the settings partition over
USB, which returns the unit to first-time setup while keeping the firmware and
history.

Erase just the settings partition, which returns the unit to first-time setup
while keeping the firmware and the whole event history:

```bash
esptool.py --chip esp32s3 -p PORT erase_region 0x9000 0x6000
```

`0x9000`/`0x6000` are the offset and size of the `nvs` partition in
`partitions.csv`. Power-cycle afterwards and the setup wizard runs again.

> Re-flashing the firmware alone does **not** help. `idf.py flash` writes the
> bootloader, partition table, OTA-data and application — nothing at `0x9000`
> — so your settings, including the password, survive it untouched.

To wipe absolutely everything instead, including the stored history, use
`idf.py -p PORT erase-flash` and then flash again.

**Factory reset.** **Settings → Configuration → Maintenance → Factory reset** erases Wi-Fi,
tokens, the password and all settings, and reboots into the wizard. The
event history is kept — clear it separately if you want it gone.

---

## 9. Building and flashing

Requires **ESP-IDF 5.3+**.

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash monitor
```

The first build fetches `led_strip` and `mdns` from the component registry.
There is nothing to configure before building and no secrets file to create
— that is the point.

After changing `sdkconfig.defaults`, delete `sdkconfig` and rebuild so the
new values are picked up.

Verified on ESP-IDF 5.3.3, xtensa-esp-elf 13.2.0: builds clean with no
warnings, ~1.2 MB, 62% of the 3 MB app partition free.

Subsequent updates go over the air from the Update tab — no cable needed.

---

### Update notices

**Settings — Firmware** shows the version you are running, and can tell you
when a newer one is published.

Switch on *Check GitHub for updates once a day* and the device asks
[the releases API](https://github.com/amirft1374/Garage-Gadget/releases) once
every 24 hours whether a newer tag exists. If it does, a notice appears at the
top of the Control tab linking to the release page.

**It never downloads or installs anything.** The device cannot tell a genuine
release from whatever answered the request — nothing here verifies a
signature — so pulling an executable off the internet and running it would be
a poor trade for saving two clicks. You download the `.bin` yourself and
upload it on the same page, exactly as before.

**Off by default**, and the setup wizard asks rather than assuming. The check
is an outbound request to a third party that reveals the device's public IP
address, and a garage opener should not start talking to the internet on its
own account without being asked. Nothing about the device, its configuration
or its door is sent — it is a plain GET for a version number.

If the check fails, the Firmware tab says why rather than failing silently:

| What it says | Meaning |
|---|---|
| `no releases published yet` | The repository exists but has no release. Not an error |
| `rate limited by GitHub (HTTP 403)` | Too many checks from your IP address. It retries in 4 hours |
| `ESP_ERR_HTTP_CONNECT` | No route to the internet, or DNS is not resolving |
| `up to date` | You are running the latest published release |

Version tags are compared numerically, so `v10.0.0` is correctly newer than
`v9.0.0`, and a `-rc` or `-beta` suffix is never announced as an upgrade.

---

## 10. How the firmware is organised

| File | Responsibility |
|---|---|
| `main.cpp` | startup order, and the door state machine: open/close detection, both alarms, auto-close |
| `app_config.h` | compile-time factory defaults. **No secrets.** |
| `app_settings.c/h` | every user setting, persisted in NVS. One field table drives load, save, JSON out and JSON in |
| `history.c/h` | the two-tier on-device store: flash event ring + PSRAM sample ring |
| `notifier.c/h` | Telegram and ntfy delivery, on a worker task |
| `door_control.c/h` | the only place the relay fires; logs every trigger everywhere it belongs |
| `wifi_manager.cpp/h` | station, setup/recovery SoftAP, scanning |
| `web_server.cpp/h` | routing, token auth, the JSON API, OTA |
| `web_assets.cpp/h` | the dashboard and wizard front-ends, as flash-resident strings |
| `influx_uploader.cpp/h` | optional InfluxDB telemetry |
| `magnet.cpp`, `relay.c`, `led_rgb.c` | hardware drivers |
| `ha_mqtt.c/h` | Home Assistant over MQTT: discovery, state, commands |
| `web_help.cpp/h` | the Help tab's content — **generated**, see `tools/gen_help.py` |
| `partitions.csv` | 2×3 MB OTA slots + the 1 MB `history` partition |
| `grafana/` | optional dashboard and alert rule templates |

Design rules worth keeping if you extend this:

- **Door safety does not depend on the network.** The polling loop starts
  before Wi-Fi and keeps running whether or not anything upstream is
  reachable.
- **Only `door_control_trigger()` touches the relay**, so every trigger is
  logged identically no matter what asked for it.
- **Adding a setting means adding one row** to the table in
  `app_settings.c` and one input with a matching `data-k` in the HTML.
  Load, save, API and UI follow automatically.

