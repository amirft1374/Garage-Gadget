# Grafana pack

Optional extras for owners who already run InfluxDB and Grafana.

**You do not need any of this.** Out of the box a Garage Gadget stores its own
history and charts it on the device's own dashboard, and it can notify your
phone directly over Telegram or ntfy. This folder is for people who want the
data in their existing stack.

| File | What it is |
|---|---|
| `garage-gadget-dashboard.json` | The dashboard. Import it into Grafana. |
| `garage-gadget-alerts.yaml` | Four optional alert rules. |

---

## 1. Point the device at InfluxDB

On the device: **Settings → Configuration → Dashboards**, switch InfluxDB on, and fill in URL,
token, organisation and bucket. Save. The Control tab's "Data backend" row
should change to `InfluxDB + on-device` within a few seconds, and
**Settings → Configuration → Notifications** is unaffected — the two are independent.

The token needs **write** access to the bucket. The dashboard needs a
separate **read** token, which is what you give Grafana.

## 2. Import the dashboard

1. Grafana → **Dashboards → New → Import**
2. Upload `garage-gadget-dashboard.json`
3. When prompted for **InfluxDB**, pick your data source
   (it must be configured with **Flux** as the query language, not InfluxQL)
4. Import

Then set the **Bucket** variable at the top of the dashboard to your bucket
name. The **Device** and **Wi-Fi SSID** dropdowns populate themselves from
the data and default to *All*, which is right if you only have one unit.

### What's on it

**At a glance** — six tiles, always visible:

| Tile | Reads |
|---|---|
| Door | live position; green CLOSED, red OPEN |
| Last report | seconds since the device last spoke. Goes orange at 3 min, red at 10 |
| Openings | how many times the door opened in the selected range |
| Immediate Alarm | the device's "should not be opening right now" alarm |
| Left Open Alarm | how many auto-close delays the door has overstayed |
| Wi-Fi signal | current RSSI, with a sparkline |

**Door activity** — a state timeline of open/closed over time (hover a band
for exact start and duration), and a bar chart of openings per day.

**Alarms and events** — alarm history as stepped lines, and a filterable,
colour-coded table of every event the device recorded.

**Device health** — collapsed by default so it stays out of the way: Wi-Fi
signal, chip temperature, free memory and uptime. Expand it when something
looks wrong.

Every panel has a description; hover the ⓘ in its corner.

## 3. Import the alert rules (optional)

`garage-gadget-alerts.yaml` has two placeholders you must replace first —
your InfluxDB data source UID and your bucket name. The header comment in
the file has copy-paste commands for PowerShell and bash, plus the three
ways to import it.

The four rules:

| Rule | Fires when | Severity |
|---|---|---|
| Garage left open | the device's Left Open Alarm counter is non-zero | warning |
| Garage opened (Immediate Alarm) | the door opened while that alarm was armed | critical |
| Garage open for more than 15 minutes | the door sensor reads open for 15 min, **regardless of whether the device's alarms are armed** | warning |
| Garage Gadget offline | no telemetry for 10 minutes | critical |

The last one is the reason to bother with Grafana alerting at all: a device
that has lost power or Wi-Fi cannot tell you so itself, and while it is
offline nothing else here is trustworthy.

Rules route through your **default notification policy**. To send one to a
specific contact point, add this to the rule at the same indentation as
`labels:`:

```yaml
notification_settings:
  receiver: My Contact Point
```

Or route on the `severity: critical` / `severity: warning` labels the rules
already carry.

### When an alert will not clear

The alarm rules read the **last** `system_event` point for their alarm and
stay firing until a zero arrives. The device sends that zero when the door
closes — but only if the alarm is still armed at that moment. Disarm an
alarm while it is firing and the clearing point is never sent, so the rule
hangs firing with nothing able to resolve it.

**Settings → Configuration → Dashboards → Send alarm state report** fixes that. It publishes
the current state of every alarm immediately:

| Alarm state | What is written |
|---|---|
| Armed and tripped | `1` (or the left-open period count) |
| Armed, not tripped | `0` |
| Disarmed | `0` — a disarmed alarm cannot be firing |

The rule picks the new point up on its next evaluation and resolves. The
button is also the quickest way to prove the whole path works end to end:
press it and watch the point land in InfluxDB.

It reports honestly rather than optimistically — with InfluxDB turned off it
says so instead of claiming to have sent something.

---

## Data model

What the firmware writes, if you want to build your own panels.
Every point is tagged `device=<device name>` and `SSID=<wi-fi network>`.

| Measurement | Field | Type | Cadence | Notes |
|---|---|---|---|---|
| `garage door position` | `value` | int | 1 s | **`1` = closed, `0` = open.** Note the spaces in the measurement name. |
| `wifi_status_garage_door` | `rssi` | int | 1 s | dBm |
| `garage_device` | `temp_c` | float | 30 s | die temperature |
| `garage_device` | `uptime_s` | int | 30 s | seconds since boot |
| `garage_device` | `heap_free` | int | 30 s | bytes |
| `system_event` | `value` | **string** | on event | tagged `event=<name>` |

`system_event` values are strings, so numeric comparisons need `|> toFloat()`
after filtering to a single `event` — several event types carry non-numeric
values and would fail conversion otherwise.

| `event` tag | `value` |
|---|---|
| `door_opened` | `"1"` |
| `door_closed` | `"1"` |
| `relay_activated` | `"web"`, `"auto_close"` or `"other"` |
| `ImmidiateAlarm` | `"1"` on raise, `"0"` on clear |
| `LeftOpenAlarm` | delay periods elapsed, `"0"` on clear |
| `auto_close_flipped` | `"enabled"` / `"disabled"` |
| `auto_close_locked` | `"on"` / `"off"` |

> The `ImmidiateAlarm` spelling is a typo that shipped in earlier firmware.
> It is kept deliberately: correcting it would break every existing alert
> rule and dashboard in the field for a cosmetic gain.

## Upgrading from the older dashboard

The earlier hand-built dashboard hard-coded the bucket, `device == "ESP32"`
and one specific SSID, and was exported in Grafana's `dashboard.grafana.app/v2`
resource format — which carries a Cloud stack namespace and internal IDs, so
it only re-imports into the stack it came from. This one is classic schema
with `__inputs`, so it imports into any Grafana 10, 11 or 12, OSS or Cloud,
and every filter is a variable. Import it alongside the old one; they have
different UIDs and will not collide.
