# Home Assistant

Two ways to connect. **Use MQTT unless you have a reason not to** — it needs
no YAML, pushes instantly, and tells Home Assistant when the device goes
offline.

| | MQTT | REST |
|---|---|---|
| Setup | tick a box on the device | paste YAML into HA |
| Entities | created automatically | you define each one |
| Updates | pushed, instant | polled, up to 30 s late |
| Device offline | shown as unavailable | stale values |
| HA must reach the device | no | **yes** |
| Admin password stored in HA | no | **yes** |
| Needs a broker | yes | no |

---

## Option A — MQTT (recommended)

### 1. Make sure you have a broker

In Home Assistant: **Settings → Add-ons → Add-on Store → Mosquitto broker**,
install, start. Then **Settings → Devices & Services** and configure the MQTT
integration if HA has not already offered it.

Create a user for the device: **Settings → People → Users → Add**, for
example `garage` with its own password. Mosquitto accepts HA users by
default.

### 2. Point the device at it

On the device: **Settings → Configuration → Home Assistant**, switch it on and fill in:

- **Broker address** — your HA host, e.g. `192.168.1.10` or
  `homeassistant.local`
- **Port** — `1883`, or `8883` with TLS on
- **Username / Password** — the user you just made

**Save all settings.** The status line underneath shows `Broker: connected`
within a few seconds, and the Control tab gains a Home Assistant row.

### 3. Nothing else

> **Home Assistant cannot override the device's master switch.** If door
> control is off on the Control tab, or the unit is still being set up, open
> and close commands from HA are refused and logged. Arm it on the device
> first — this is deliberate, so a home-automation rule cannot move a door
> that someone disabled on purpose.

Home Assistant creates one device with these entities:

| Entity | Type | |
|---|---|---|
| Garage Door | `cover` | open/close, with the garage device class |
| | | *refused while door control is off on the device* |
| Door sensor voltage | sensor | mV, diagnostic |
| Wi-Fi signal | sensor | dBm, diagnostic |
| Chip temperature | sensor | °C, diagnostic |
| Immediate Alarm | binary sensor | `problem` class |
| Left Open Alarm | binary sensor | `problem` class |
| Automatic close | switch | config |
| Immediate Alarm armed | switch | config |
| Left Open Alarm armed | switch | config |
| Auto-close delay | number | 1–120 min, config |

Changes made in the web UI appear in HA and vice versa — both publish state
after every change.

### Notifications

You do not need the device's Telegram/ntfy setup if you use HA. Automate on
the alarm entities instead:

```yaml
automation:
  - alias: Garage left open
    trigger:
      - platform: state
        entity_id: binary_sensor.garage_left_open_alarm
        to: "on"
    action:
      - service: notify.mobile_app_your_phone
        data:
          title: Garage left open
          message: The garage door has been open past its auto-close delay.
```

The device's own notifications keep working alongside this. Running both is
reasonable: HA goes down sometimes, and the device does not depend on it.

### Topics

Everything lives under `garagegadget/<id>/`, where `<id>` is
`garage_` plus the last three bytes of the MAC. Watch it all with:

```bash
mosquitto_sub -h 192.168.1.10 -u garage -P yourpassword -t 'garagegadget/#' -v
```

`garagegadget/<id>/status` is `online` / `offline`, retained, set as the
last will — which is how HA knows the device has dropped.

### Troubleshooting

| Status line | Meaning |
|---|---|
| `cannot reach broker` | wrong address or port, or a firewall between them |
| `broker refused the connection (code 5)` | wrong username or password |
| `broker refused the connection (code 4)` | bad credentials format |
| `connecting…` and staying there | broker address resolves but nothing is listening |

If entities never appear but the status says connected, check the MQTT
integration is actually set up in HA (the add-on alone is not enough) and
that its discovery prefix is the default `homeassistant`.

---

## Option B — REST, no broker

Works with the device exactly as it is; everything is on the HA side. Pays
for that with polling lag, and your admin password sitting in HA's config.

Put the password in `secrets.yaml`:

```yaml
# secrets.yaml
garage_password: your-admin-password
garage_host: 192.168.1.42
```

Then in `configuration.yaml`:

```yaml
rest:
  - resource: !secret garage_status_url
    scan_interval: 15
    sensor:
      - name: Garage door voltage
        value_template: "{{ value_json.doorMv }}"
        unit_of_measurement: mV
        device_class: voltage
      - name: Garage Wi-Fi signal
        value_template: "{{ value_json.rssi }}"
        unit_of_measurement: dBm
        device_class: signal_strength
      - name: Garage chip temperature
        value_template: "{{ value_json.tempC }}"
        unit_of_measurement: "°C"
        device_class: temperature
    binary_sensor:
      - name: Garage door
        value_template: "{{ not value_json.doorClosed }}"
        device_class: garage_door
      - name: Garage Left Open Alarm
        value_template: "{{ value_json.leftOpenPeriods > 0 }}"
        device_class: problem
      - name: Garage Immediate Alarm
        value_template: "{{ value_json.immediateTripped }}"
        device_class: problem

rest_command:
  garage_trigger:
    url: !secret garage_door_url
    method: POST

cover:
  - platform: template
    covers:
      garage_door:
        friendly_name: Garage Door
        device_class: garage
        value_template: "{{ not is_state('binary_sensor.garage_door', 'on') }}"
        open_cover:
          - condition: state
            entity_id: binary_sensor.garage_door
            state: "off"
          - service: rest_command.garage_trigger
        close_cover:
          - condition: state
            entity_id: binary_sensor.garage_door
            state: "on"
          - service: rest_command.garage_trigger
```

Add the two composed URLs to `secrets.yaml`:

```yaml
garage_status_url: http://192.168.1.42/api/status
garage_door_url: http://192.168.1.42/api/door?code=your-admin-password
```

Notes:

- `/api/status` needs no authentication, so the sensors work without the
  password. Only the trigger does.
- The `condition` steps in the cover stop HA pulsing the opener when the
  door is already in the requested state. The opener has one button, so a
  redundant pulse moves the door the *wrong* way.
- Give the device a **DHCP reservation**. These URLs hard-code its address.
- Use `http://`, not `https://` — the device has no certificate.

The password travels in a URL here, which is why MQTT is preferred. It will
appear in HA logs on error.
