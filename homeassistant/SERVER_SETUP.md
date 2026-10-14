# Home server tasks — connecting a Garage Gadget to Home Assistant

**This is a handoff brief for an AI assistant working on the Home Assistant
server.** Everything on the ESP32 device itself is already built and
flashed; nothing here requires touching the device's firmware. The tasks
below are all server-side.

Work through the phases in order. Phase 1 and 2 are required; 3 onward are
optional but recommended.

---

## Context you need

The Garage Gadget is an ESP32-S3 that monitors and controls a garage door.
It speaks **MQTT with Home Assistant Discovery**: it connects *out* to an
MQTT broker and publishes retained discovery payloads, so Home Assistant
creates all entities by itself. **No YAML is needed to create entities.**

The device dials out, so Home Assistant never has to reach it. Do not spend
time on firewall rules from HA to the device.

### Values to confirm with the user before starting

| Value | Likely | How to confirm |
|---|---|---|
| Device IP | `192.168.1.50` (yours will differ) | shown in the device's web UI header |
| Device web UI | `http://192.168.1.50` | plain HTTP, no certificate |
| MQTT client/device id | `garage_a2df24` | last 3 bytes of the device MAC; appears in broker logs on connect |
| MQTT topic root | `garagegadget/garage_a2df24/` | |
| HA host | ask | |
| HA install type | ask | **OS/Supervised** has an add-on store; **Container/Core** does not |

The device's admin password is known only to the user. You should not need
it for the MQTT path.

> If the user's HA is **Container or Core**, there is no add-on store — run
> Mosquitto as a separate container instead. Ask which they have before
> assuming.

---

## Phase 1 — MQTT broker

### 1.1 Install Mosquitto

**HA OS / Supervised:** Settings → Add-ons → Add-on Store → *Mosquitto
broker* → Install → Start. Enable "Start on boot" and "Watchdog".

**Container / Core:** run `eclipse-mosquitto` alongside HA. Minimum config:

```conf
# mosquitto.conf
listener 1883
allow_anonymous false
password_file /mosquitto/config/passwords
persistence true
persistence_location /mosquitto/data/
```

Create the password file with `mosquitto_passwd -c /mosquitto/config/passwords garage`.

### 1.2 Create a dedicated user for the device

Do **not** reuse the user's own HA login.

- **With the add-on:** Settings → People → Users → Add user. Name `garage`,
  give it a strong password, mark it as **not** an administrator. The
  Mosquitto add-on authenticates HA users by default.
- **Standalone broker:** `mosquitto_passwd` as above.

Record the username and password — the user must type them into the device.

### 1.3 Configure the MQTT integration in HA

Settings → Devices & Services → Add Integration → **MQTT**. HA usually
auto-discovers the add-on and offers it; accept.

Leave the discovery prefix at the default `homeassistant`. The device
hard-codes that prefix.

**Verify before moving on:**

```bash
mosquitto_sub -h localhost -u garage -P '<password>' -t '$SYS/broker/version' -C 1
```

A version string means the broker is up and the credentials work.

---

## Phase 2 — Connect the device

### 2.1 User action (not yours)

Ask the user to open `http://<device-ip>` → **Settings** → unlock with their
admin password → **Configuration** → **Home Assistant** section, and enter:

- Switch **on**
- **Broker address** — the HA host IP (use the IP, not `.local`; mDNS from
  an ESP32 is less reliable than a plain address)
- **Port** — `1883`
- **Username / Password** — from 1.2
- **TLS** — off, unless the broker was configured for it

Then **Save all settings**.

The device's status line should read `Broker: connected` within ~5 seconds.
If it does not, the message is specific — relay it; see Troubleshooting.

### 2.2 Verify from the server

```bash
mosquitto_sub -h localhost -u garage -P '<password>' -t 'garagegadget/#' -v
```

Expect, immediately on device connect:

- `garagegadget/<id>/status` → `online`
- `garagegadget/<id>/cover/state` → `closed` or `open`
- several `garagegadget/<id>/switch/...` and `.../number/...` values

And discovery configs:

```bash
mosquitto_sub -h localhost -u garage -P '<password>' -t 'homeassistant/#' -v | grep garage
```

### 2.3 Confirm the entities exist

Settings → Devices & Services → MQTT → Devices. There should be **one
device** with **10 entities**:

| Entity | Type |
|---|---|
| Garage Door | `cover`, device class `garage` |
| Door sensor voltage | sensor, mV |
| Wi-Fi signal | sensor, dBm |
| Chip temperature | sensor, °C |
| Immediate Alarm | binary sensor, `problem` |
| Left Open Alarm | binary sensor, `problem` |
| Automatic close | switch |
| Immediate Alarm armed | switch |
| Left Open Alarm armed | switch |
| Auto-close delay | number, 1–120 min |

Note the actual entity IDs HA assigns — they are used below and may be
suffixed if names collide.

### 2.4 Functional test

**Warn the user first: this moves a real garage door.** Confirm nobody and
nothing is underneath, then toggle the `cover` entity and check the door
moves and the state updates within a second or two.

Then test offline detection: ask the user to unplug the device briefly. All
entities should go **unavailable** within ~30 seconds (the keepalive), then
return. If they stay available showing stale values, the last-will is not
working — check the broker is honouring retained messages.

---

## Phase 3 — Dashboard

Add a card. Minimal version:

```yaml
type: entities
title: Garage
entities:
  - entity: cover.garage_door
  - entity: binary_sensor.garage_left_open_alarm
  - entity: binary_sensor.garage_immediate_alarm
  - type: section
    label: Settings
  - entity: switch.garage_automatic_close
  - entity: number.garage_auto_close_delay
  - entity: switch.garage_left_open_alarm_armed
```

Substitute the real entity IDs from 2.3.

---

## Phase 4 — Notifications to the user's phone

The device can already notify Telegram/ntfy by itself, independently of HA.
Ask the user whether they want HA notifications **as well** or **instead**.
Running both is defensible — HA goes down sometimes, and the device does not
depend on it — but it does mean two alerts per event.

Requires the **Home Assistant Companion app** installed and logged in, which
creates a `notify.mobile_app_<device>` service.

```yaml
automation:
  - alias: Garage left open
    mode: single
    trigger:
      - platform: state
        entity_id: binary_sensor.garage_left_open_alarm
        to: "on"
    action:
      - service: notify.mobile_app_<device>
        data:
          title: Garage left open
          message: The door has been open past its auto-close delay.
          data:
            # Lets the user close it straight from the notification.
            actions:
              - action: CLOSE_GARAGE
                title: Close it

  - alias: Garage close from notification
    trigger:
      - platform: event
        event_type: mobile_app_notification_action
        event_data:
          action: CLOSE_GARAGE
    action:
      - service: cover.close_cover
        target:
          entity_id: cover.garage_door

  - alias: Garage device offline
    trigger:
      - platform: state
        entity_id: cover.garage_door
        to: unavailable
        for: "00:05:00"
    action:
      - service: notify.mobile_app_<device>
        data:
          title: Garage monitor offline
          message: >-
            No contact for 5 minutes. The door state shown in Home Assistant
            is no longer trustworthy.
```

That last automation is the one HA adds that the device genuinely cannot do
for itself — a device that has lost power cannot report that it has lost
power.

---

## Phase 5 — Access from the user's phone

Covered in detail below under "Answering the user's questions". In short:
the Companion app on the home Wi-Fi needs nothing extra; from outside the
house it needs one of Nabu Casa, Tailscale, or a reverse proxy. Ask which
they want before configuring anything. **Do not port-forward Home Assistant
directly to the internet.**

---

## Phase 6 — Housekeeping

- **DHCP reservation for the device.** Not strictly required for MQTT (it
  dials out), but it keeps the web UI at a stable address. Do the same for
  the HA host, since the device stores the broker address.
- **Back up HA** after the integration works: Settings → System → Backups.
- If the user also wants InfluxDB/Grafana, that is a separate and
  independent path — see the `grafana/` folder in the device's repo. It does
  not conflict with MQTT; both can run at once.

---

## Troubleshooting

Work from the device's status line, which is specific:

| Device says | Cause | Fix |
|---|---|---|
| `cannot reach broker` | wrong IP/port, or broker not listening | check the add-on is started; confirm the HA host IP; check port 1883 |
| `broker refused the connection (code 5)` | bad username or password | re-create the user; check the add-on's auth settings |
| `broker refused the connection (code 4)` | malformed credentials | re-enter them |
| `connecting…` forever | address resolves, nothing listening | wrong host, or Mosquitto not running |
| `connected` but no entities in HA | MQTT **integration** not configured (the add-on alone is not enough), or non-default discovery prefix | Settings → Devices & Services → add MQTT |

Broker-side:

```bash
# Is the device connecting at all?
mosquitto_sub -h localhost -u garage -P '<pw>' -t '$SYS/broker/log/#' -v
```

If entities appeared once and then vanished, someone cleared the retained
discovery topics. Power-cycle the device; it republishes discovery on every
connect.

---

## Things NOT to do

- **Do not port-forward** Home Assistant or the device to the internet.
- **Do not use the device's REST API** (`/api/door?code=<password>`) if MQTT
  is working. It puts the admin password in HA's config and logs. It exists
  only for installs without a broker; see `README.md` in this folder.
- **Do not set `allow_anonymous true`** on the broker.
- **Do not change the MQTT discovery prefix** from `homeassistant` — the
  device hard-codes it.
- **Do not assume the door state is trustworthy while the device is
  unavailable.** That is what Phase 4's offline automation is for.

---

## Definition of done

- [ ] Mosquitto installed, running, starts on boot
- [ ] Dedicated non-admin MQTT user created
- [ ] MQTT integration configured in HA
- [ ] Device reports `Broker: connected`
- [ ] One device with 10 entities visible in HA
- [ ] Opening/closing from HA works and state updates
- [ ] Entities go unavailable when the device is unplugged
- [ ] Dashboard card added
- [ ] Phone notification for Left Open Alarm fires
- [ ] Offline automation configured
- [ ] DHCP reservations for both the device and the HA host
- [ ] HA backup taken
