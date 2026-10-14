/*
 * web_help.cpp - GENERATED FILE. Do not edit by hand.
 *
 * Built from the project Markdown by tools/gen_help.py so the Help
 * tab cannot drift from the documentation that ships in the repo.
 * Re-run that script after changing any .md file.
 */

#include "web_help.h"

static const char k_help_0[] = R"HELP(<ul><li><strong>Door position sensing</strong> from a reed or hall sensor, read as an analogue voltage with a user-settable threshold, so it works with whatever sensor and wiring the installation happens to have.</li><li><strong>Door control</strong> — a relay pulse across the opener's manual button, behind a master switch that ships <strong>off</strong>. See Arming door control.</li><li><strong>Auto-close</strong> — shuts the door after a configurable delay, with a hard lock-off switch for when you genuinely want it left open.</li><li><strong>Two alarms</strong> — <em>Immediate</em> (the door should not be opening right now) and <em>Left Open</em> (it has overstayed its delay, and keeps escalating).</li><li><strong>Phone notifications</strong> sent straight from the device over Telegram or ntfy. No broker, no cloud account of ours, nothing to host.</li><li><strong>Its own dashboard and history</strong> — the last 10 days of charts and a long-running event log, served from the device, with no external database required.</li><li><strong>Home Assistant</strong> over MQTT, with auto-discovery: HA builds the garage door, sensors and switches by itself. See <code>homeassistant/</code>.</li><li><strong>Optional InfluxDB + Grafana</strong> for owners who already run them. See <code>grafana/</code>.</li><li><strong>OTA firmware update</strong> by dragging a <code>.bin</code> onto the Update tab, with an optional daily check that tells you when a new release exists.</li></ul>)HELP";

static const char k_help_1[] = R"HELP(<p>Built for <strong>ESP32-S3</strong> with <strong>16 MB flash</strong> and <strong>8 MB octal PSRAM</strong> (an <code>-R8</code> part), on ESP-IDF <strong>5.3</strong>.</p>
<div class="tblWrap"><table><thead><tr><th>Function</th><th>GPIO</th><th>Direction</th><th>Notes</th></tr></thead><tbody><tr><td>Door position sensor</td><td>6</td><td><strong>analogue in</strong> (ADC1 ch5)</td><td>≥ threshold = closed. See below.</td></tr><tr><td>Relay control</td><td>18</td><td>pulse output, active low</td><td>idles as a high-Z input for safety</td></tr><tr><td>RGB status LED (SK6812)</td><td>48</td><td>output</td><td>one LED, RMT driven</td></tr></tbody></table></div>
<p>The relay pin sits as an <strong>input</strong> when idle, so a reset or a crash cannot leave the opener's button held down. It is switched to a low output for a brief pulse and immediately released.</p>
<p><strong>Pulse length</strong> defaults to 100 ms and is adjustable in <strong>Settings → Configuration → Device → Relay pulse length</strong> (20–2000 ms). Raise it if your opener sometimes ignores the press. Do not raise it far: many openers treat a multi-second hold as a request to enter programming or limit-setting mode, which is why the upper bound exists.</p>
<h3>Door sensor — analogue, not digital</h3>
<p>The sensor pin is read as a <strong>voltage</strong>, not a logic level. Reed switches and hall sensors wired through a pull-up or a divider often do not swing all the way to the rails, and a plain digital read then either works by luck or not at all. Reading millivolts means one firmware covers every wiring variant and the trip point becomes a setting rather than a property of the hardware.</p>
<ul><li><strong>At or above the threshold → CLOSED.</strong> Below it → OPEN.</li><li>Default threshold <strong>1800 mV</strong>, adjustable in <strong>Settings → Configuration → Door sensor → Door-closed threshold</strong>.</li><li>A <strong>±150 mV dead-band</strong> around the threshold stops a sensor resting near the line from chattering and filling the event log with phantom opens.</li><li>Each reading is the <strong>median of 5 samples</strong>, so one wild ADC sample cannot flip the state.</li><li>ADC1 is used deliberately: ADC2 shares hardware with the Wi-Fi radio and its reads fail whenever Wi-Fi is active, which here is always.</li></ul>
<h3>Calibration</h3>
<p><strong>Settings → Configuration → Door sensor.</strong> Rather than asking you to work out a threshold, the device measures what your sensor actually does:</p>
<ol><li><strong>Close the door</strong> fully, let it settle, press <strong>Capture closed</strong>.</li><li><strong>Open the door</strong>, let it settle, press <strong>Capture open</strong>.</li></ol>
<p>Each capture averages 64 readings over about a second and reports how much the value wandered while measuring. The device then classifies by <strong>nearest reference</strong>: whichever captured reading the live value is closer to wins. The trip point is simply the midpoint of the two.</p>
<p>Because it works on "nearest", <strong>polarity does not matter</strong> — a sensor that reads <em>lower</em> when the door is shut works exactly as well as one that reads higher. You do not have to tell it which way round your sensor is.</p>
<p>The dead-band is derived from the measured separation (a quarter of it, clamped to 30–300 mV) instead of being a fixed number, so a sensor with a small swing gets a proportionally tighter band.</p>
<p>The panel shows a live position strip: your two captured points, the trip point between them, and a needle for the current reading. If the needle sits firmly in the green half when the door is shut and the red half when it is open, you are done.</p>
<p><strong>If calibration is refused</strong>, the two readings were less than 200 mV apart. That means the sensor is not actually distinguishing the two positions — usually a floating input, a missing pull-up, or too large an air gap. The device keeps using the manual trip point until you fix it and re-capture.</p>
<p><strong>Manual alternative.</strong> You can still type a <strong>Trip point</strong> directly and skip calibration; in that case higher means closed and the dead-band is the fixed <code>APP_DOOR_HYSTERESIS_MV</code>.</p>
<p>The Control tab shows the live reading at all times as <code>1882 mV · raw 2232 · trip 1800</code>, right above the chip temperature.</p>
<p>If the value reads <code>(uncal.)</code>, the chip had no ADC calibration data in eFuse and the voltage is a linear estimate — accurate to a few percent, which is still fine for a threshold comparison.</p>
<h3>Status LED</h3>
<div class="tblWrap"><table><thead><tr><th>Colour</th><th>Meaning</th></tr></thead><tbody><tr><td>dim white</td><td>booting</td></tr><tr><td>blue, breathing</td><td>first-time setup — join the setup Wi-Fi</td></tr><tr><td>magenta, slow blink</td><td>configured, but cannot reach Wi-Fi</td></tr><tr><td>dim green</td><td>closed, online, nothing to report</td></tr><tr><td>amber/red, fast blink</td><td>door open</td></tr><tr><td>red, fast blink</td><td>alarm</td></tr><tr><td>brief blue flash</td><td>the relay just fired</td></tr></tbody></table></div>
<h3>Other boards</h3>
<p>Change the GPIO constants at the top of <code>main/magnet.cpp</code>, <code>main/relay.c</code> and <code>main/led_rgb.c</code>. If you move the door sensor, it must land on an <strong>ADC1</strong> pin (GPIO1–GPIO10 on the S3); update both <code>MAGNET_ADC_CHANNEL</code> and the comment naming the GPIO. If your board has quad PSRAM or none, adjust <code>sdkconfig.defaults</code> — the comments there say exactly what to change. With no PSRAM the firmware still runs and automatically drops from 10 days of chart history to 2.</p>)HELP";

static const char k_help_2[] = R"HELP(<p>No cables, no config files.</p>
<ol><li><strong>Power the unit.</strong> The status LED breathes blue.</li><li><strong>On your phone, join the Wi-Fi network <code>GarageGadget-XXXX</code>.</strong> The password is <code>garagesetup</code>. <code>XXXX</code> is from the device's MAC, so several units on one bench stay distinguishable.</li><li><strong>Open <code>http://192.168.4.1</code>.</strong> Most phones offer this automatically as a "sign in to network" notification.</li><li><strong>Work through the five steps:</strong></li></ol>
<div class="tblWrap"><table><thead><tr><th>Step</th><th>Required?</th><th></th></tr></thead><tbody><tr><td>1. Wi-Fi</td><td><strong>yes</strong></td><td>pick your network from the scan list, or type it</td></tr><tr><td>2. Device name</td><td><strong>yes</strong></td><td>becomes <code>http://&lt;name&gt;.local</code> and tags your data</td></tr><tr><td>3. InfluxDB / Grafana</td><td>no</td><td>skip unless you already run them</td></tr><tr><td>4. Notifications</td><td>no</td><td>Telegram and/or ntfy — see §5</td></tr><tr><td>5. Admin password</td><td><strong>yes</strong></td><td>4–32 characters</td></tr></tbody></table></div>
<ol><li><strong>Finish.</strong> The device joins your Wi-Fi. Reconnect your phone to your normal network and open <code>http://&lt;device-name&gt;.local</code>.</li></ol>
<p>The door cannot be moved during any of this — see Arming door control.</p>
<p>If the Wi-Fi details were wrong the device keeps its setup network up until setup actually completes, so you can correct them. You cannot lock yourself out.</p>)HELP";

static const char k_help_3[] = R"HELP(<p><code>http://&lt;device-name&gt;.local</code> (or the IP shown in the header).</p>
<h3>Arming door control</h3>
<p><strong>A new device will not move the door.</strong> The relay master switch on the <strong>Control</strong> tab defaults to off, and until you turn it on the relay is never energised — not by the button on this page, not by auto-close, not by Home Assistant.</p>
<p>That default is deliberate. A unit on the bench is often already wired to a real opener, and before calibration the sensor reads whatever a floating pin feels like — usually "open". Auto-close is on by default. Without this switch, the first thing a half-configured unit does is drive the door while the installer is stood underneath it with a screwdriver.</p>
<p>Turn it on once the unit is mounted, the sensor is calibrated and the door state on screen matches the real door. The setting is stored in NVS, so a power cut cannot silently re-arm a door you switched off on purpose.</p>
<p>The firmware also refuses to fire while the device is still being set up, or while it is showing its setup hotspot, whatever the switch says. A unit being configured is a unit someone is standing next to.</p>
<p>One case is deliberately <strong>not</strong> blocked: <em>rescue</em> mode, where a configured device raised its hotspot because the router went away. That unit is fully set up, and refusing to shut someone's garage because their Wi-Fi is down would be its own kind of failure.</p>
<p>A sidebar on the left holds the four sections; on a phone it becomes a strip along the top. It also shows the device name and the live door state, so the answer to "is my garage shut" is on screen whatever you are looking at.</p>
<p><strong>Control</strong> — no password needed. A garage graphic whose door panel slides open and shut with the real one, the trigger button, both alarm lamps, the auto-close settings, and device vitals: door sensor voltage and raw ADC count, chip temperature, Wi-Fi signal, uptime.</p>
<p><strong>History</strong> — 24 h / 3 d / 7 d / 10 d of door position, Wi-Fi signal and chip temperature, plus the full event log. All from the device's own storage.</p>
<p><strong>Settings</strong> — three sub-tabs:</p>
<ul><li><em>Configuration</em> — split again by a second bar into <strong>Wi-Fi</strong>, <strong>Device</strong>, <strong>Door sensor</strong>, <strong>Dashboards</strong> (InfluxDB and Grafana), <strong>Home Assistant</strong>, <strong>Notifications</strong>, <strong>Security</strong> (trusted devices and the admin password) and <strong>Maintenance</strong>. One section shows at a time, but <strong>Save all settings</strong> stays pinned to the bottom of the page and writes every section at once — you do not need to save before switching.</li><li><em>Terminal</em> — the live log, the same text you would see on the serial port.</li><li><em>Firmware</em> — drag a <code>.bin</code> on to update over the air. The device validates the image before switching to it and falls back to the running firmware if it does not verify.</li></ul>
<p><strong>Help</strong> — the whole manual, on the device. Categorised, one topic per page, and readable <strong>without signing in</strong>: the recovery instructions are needed exactly when you cannot get in, and none of it is specific to your unit. Every settings section has a <strong>?</strong> beside its heading that jumps straight to the page explaining it.</p>
<p>The Help content is generated from the Markdown in this repo by <code>tools/gen_help.py</code>, so it can never drift from the shipped documentation. Re-run that script after editing any <code>.md</code> file, then rebuild.</p>
<p>Secrets are stripped on the way in: the generator masks anything shaped like an API token, so a real value pasted into the docs while debugging cannot end up published in every unit's built-in manual.</p>
<p>History and Settings ask for the admin password, as does anything that moves the door. You stay signed in for 12 hours; if the device reboots in the meantime the page simply asks again and resumes what you were doing.</p>
<blockquote>Pressing <strong>Send test notification</strong> blocks the UI for a few seconds while the device actually contacts Telegram/ntfy. That is expected — it is reporting a real delivery result, not a guess.</blockquote>
<h3>Giving the device a fixed address</h3>
<p>By default the device takes whatever address DHCP offers, which is fine until something needs to point at it: a Home Assistant entry, a bookmark, a port-forward. Two ways to pin it down:</p>
<p><strong>A reservation in your router</strong> (preferred). Find the device's MAC in the router's client list and reserve an address for it. Nothing is stored on the device, and it still works if you move it to a different network.</p>
<p><strong>Settings → Configuration → Device → Fixed IP address</strong>, if your router cannot do reservations or is not yours to configure. Fill in the address and your gateway; the mask defaults to <code>255.255.255.0</code> and a blank DNS means "ask the gateway". It takes effect on the next restart.</p>
<div class="tblWrap"><table><thead><tr><th>Field</th><th>Required</th><th></th></tr></thead><tbody><tr><td>Fixed IP address</td><td>—</td><td>blank means DHCP</td></tr><tr><td>Gateway</td><td><strong>yes, if an IP is set</strong></td><td>your router</td></tr><tr><td>Subnet mask</td><td>no</td><td>blank = <code>255.255.255.0</code></td></tr><tr><td>DNS server</td><td>no</td><td>blank = same as the gateway</td></tr></tbody></table></div>
<p>The obvious danger with a hand-set address is that a wrong one still joins the Wi-Fi perfectly and is simply unreachable, which is a bad failure to have on a device bolted to a garage ceiling. So after the interface comes up the device pings its gateway, and if nothing answers it abandons the fixed address and takes a DHCP lease for that boot. Your setting is left exactly as you typed it and a warning appears at the top of the dashboard saying what happened. A typo costs a reboot, not a ladder.</p>
<p>A gateway on the wrong subnet, a duplicate address already in use, or a mask that excludes the gateway all land in the same place: the ping fails and you get the lease. The one case it cannot catch is an address that works but belongs to something else, which is what reservations exist to prevent.</p>)HELP";

static const char k_help_4[] = R"HELP(<p>By default, entirely on the device — two tiers, chosen to match how the data is actually used.</p>
<div class="tblWrap"><table><thead><tr><th></th><th>Events</th><th>Samples</th></tr></thead><tbody><tr><td><strong>What</strong></td><td>door opened/closed, relay pulses, alarms, boots, Wi-Fi drops</td><td>door state, Wi-Fi signal, chip temperature</td></tr><tr><td><strong>When</strong></td><td>as they happen</td><td>once a minute</td></tr><tr><td><strong>Where</strong></td><td>1 MB flash partition</td><td>PSRAM</td></tr><tr><td><strong>Survives reboot</strong></td><td><strong>yes</strong></td><td>no</td></tr><tr><td><strong>Capacity</strong></td><td>~65,000 records — over a year in practice</td><td>10 days (2 days without PSRAM)</td></tr></tbody></table></div>
<p>Events go to flash because losing the record of your garage opening at 3 a.m. is unacceptable. The per-minute sample trace is chart texture, not a record, so it stays in RAM — which also means routine polling causes no flash wear at all. Flash writes happen only when something genuinely occurs.</p>
<p>The event log is a ring buffer of 16-byte records with a CRC on each; the head is recovered on boot by reading one record per sector, so a power cut mid-write costs at most the record being written.</p>
<p><strong>Settings</strong> live in NVS, separately, and are never touched by history writes.</p>
<p>Both tiers are readable from the History tab, and erasable from <strong>Settings → Configuration → Maintenance</strong>.</p>
<p>If you would rather have the data in InfluxDB and Grafana, turn it on in Settings — the on-device history keeps working alongside it. See <code>grafana/</code>.</p>)HELP";

static const char k_help_5[] = R"HELP(<p>The device posts directly to a public push endpoint over HTTPS. Set up either or both under <strong>Settings → Configuration → Notifications</strong>.</p>
<h3>ntfy — easiest, no account</h3>
<ol><li>Install <strong>ntfy</strong> (<a href="https://apps.apple.com/app/ntfy/id1625396347" target="_blank" rel="noopener">iOS</a> / <a href="https://play.google.com/store/apps/details?id=io.heckel.ntfy" target="_blank" rel="noopener">Android</a>).</li><li>Pick a topic name <strong>nobody could guess</strong> — e.g. <code>garage-7f3a91c4e2</code>. Anyone who knows the topic can read your alerts, and there is no password on a public ntfy topic.</li><li>Subscribe to that topic in the app.</li><li>On the device: switch <strong>ntfy</strong> on, leave the server as <code>https://ntfy.sh</code>, enter the topic, <strong>Save all settings</strong>, then <strong>Send test notification</strong>.</li></ol>
<p>Self-hosting ntfy works too — put your own URL in the server field.</p>
<h3>Telegram</h3>
<p><strong>Full walkthrough, including how to manage the bot afterwards and what every error message means: TELEGRAM.md.</strong> Short version:</p>
<ol><li>Message <strong>@BotFather</strong>, send <code>/newbot</code>, follow the prompts, copy the whole token (<code>123456789:AAH…</code>, colon included).</li><li><strong>Open your new bot and press START.</strong> Telegram blocks bots from messaging you until you message them first — this is the step people miss.</li><li>Message <strong>@userinfobot</strong> to get your numeric chat ID.</li><li>Sanity-check both in a browser before touching the device: <code>https://api.telegram.org/bot&lt;TOKEN&gt;/getMe</code> should return <code>"ok":true</code>.</li><li>On the device: switch <strong>Telegram</strong> on, paste the token and chat ID, <strong>Save all settings</strong>, then <strong>Send test notification</strong>.</li></ol>
<h3>Multiple recipients</h3>
<p><strong>Settings → Configuration → Notifications → Recipients.</strong> Add as many as eight chat IDs, each with a nickname so the list stays readable. Every recipient receives every alert you have enabled.</p>
<p>Delivery is per-recipient: one bad chat ID cannot stop the others from being told the garage is open. A partial failure reports which nickname failed and why.</p>
<p>Press <strong>Find chat IDs</strong> to have the device ask Telegram who has messaged the bot recently, then tap the right entry. This is the reliable way to get a chat ID — the number before the colon in your bot token is the <em>bot's</em> id, and using it by mistake produces a "bot can't send messages to bot" error that explains nothing.</p>
<p>An existing single chat ID is migrated into the list automatically on the first boot after updating; nothing to redo.</p>
<h3>Choosing what to be told about</h3>
<div class="tblWrap"><table><thead><tr><th>Toggle</th><th>Default</th><th></th></tr></thead><tbody><tr><td>Alarms</td><td><strong>on</strong></td><td>the ones that matter</td></tr><tr><td>Door opens</td><td>off</td><td>noisy on a normal household door</td></tr><tr><td>Door closes</td><td>off</td><td>but a close that <em>clears an alarm</em> is always sent</td></tr><tr><td>Boot and Wi-Fi recovery</td><td><strong>on</strong></td><td>so you know the device came back</td></tr></tbody></table></div>
<p>The device cannot tell you it is offline. If that matters, use the Grafana offline rule in <code>grafana/</code>.</p>)HELP";

static const char k_help_6[] = R"HELP(<p>Telegram will not let a program message you directly. Instead you create a <strong>bot</strong> — an account controlled by an API token — and the device sends messages <em>as that bot</em>.</p>
<p>Three things have to line up:</p>
<div class="tblWrap"><table><thead><tr><th>Thing</th><th>What it is</th><th>Where it comes from</th></tr></thead><tbody><tr><td><strong>Bot token</strong></td><td>The bot's password. <code>1234567890:AA**…</code> — a numeric id, a colon, then a ~35-character key.</td><td>BotFather</td></tr><tr><td><strong>Chat ID</strong></td><td>Which conversation to send to. For a private chat with yourself this is your own numeric user ID.</td><td>@userinfobot</td></tr><tr><td><strong>You messaging the bot first</strong></td><td>Telegram blocks bots from opening conversations. Until you send it something, it cannot reply.</td><td>You, in the Telegram app</td></tr></tbody></table></div>
<p>The third one is not optional and it is the step almost everyone misses.</p>)HELP";

static const char k_help_7[] = R"HELP(<ol><li>In Telegram, search for <strong>@BotFather</strong> (the one with the blue verified tick) and open a chat with it.</li><li>Send <code>/newbot</code>.</li><li>It asks for a <strong>name</strong> — the display name shown on messages. Anything: <code>My Garage</code>.</li><li>It asks for a <strong>username</strong> — must be unique across all of Telegram and must end in <code>bot</code>. For example <code>john_garage_9174_bot</code>.</li><li>BotFather replies with a congratulations message containing a line like:</li></ol>
<pre><code>Use this token to access the HTTP API:
1234567890:AA*********************************</code></pre>
<p><strong>Copy the whole token in one action</strong> — tap it in Telegram (it is tappable and copies cleanly), or select from the first digit to the last character. The colon is part of the token. Do not retype it by hand.</p>
<blockquote>Keep this token secret. Anyone who has it can post as your bot.</blockquote>)HELP";

static const char k_help_8[] = R"HELP(<p><strong>Do not skip this.</strong></p>
<ol><li>BotFather's message includes a <code>t.me/&lt;your_bot_username&gt;</code> link. Tap it.</li><li>Your bot's chat opens with a <strong>START</strong> button. Press it (or send <code>hi</code>).</li></ol>
<p>Until you do this, every send attempt fails with <em>"chat not found"</em> or <em>"Forbidden: bot can't initiate conversation with a user"</em>.</p>)HELP";

static const char k_help_9[] = R"HELP(<ol><li>Search for <strong>@userinfobot</strong> and open it.</li><li>Press <strong>START</strong>.</li><li>It replies with your details, including <code>Id: 123456789</code>.</li></ol>
<p>That number is your chat ID. It is your own Telegram user ID — 9 or 10 digits, no colon.</p>
<blockquote>Do not confuse it with the number before the colon in the bot token. That one is the <em>bot's</em> id, not yours. They are different numbers.</blockquote>)HELP";

static const char k_help_10[] = R"HELP(<p>This is the fastest way to tell a bad token from a bad chat ID. Open a browser and visit:</p>
<pre><code>https://api.telegram.org/botTOKEN/getMe</code></pre>
<p>…where you replace the word <code>TOKEN</code> with your actual token. <strong>Delete the placeholder entirely — do not keep any angle brackets.</strong> A finished URL looks like this, with no space and no <code>/</code> between <code>bot</code> and the digits:</p>
<pre><code>https://api.telegram.org/bot1234567890:AA*********************************/getMe</code></pre>
<p>Read the reply carefully — the three outcomes mean very different things:</p>
<div class="tblWrap"><table><thead><tr><th>Reply</th><th>Meaning</th><th>What to do</th></tr></thead><tbody><tr><td><code>"ok":true</code> with a <code>username</code></td><td>The token is <strong>valid</strong>. Check it is the bot you expect.</td><td>Move on to the chat ID test below.</td></tr><tr><td><code>"error_code":404, "Not Found"</code></td><td>The <strong>URL</strong> is malformed — Telegram could not find a token in it at all. Almost always the placeholder or its angle brackets were left in.</td><td>Rebuild the URL exactly as above. This is <em>not</em> a token problem.</td></tr><tr><td><code>"error_code":401, "Unauthorized"</code></td><td>The token reached Telegram and was <strong>rejected</strong>.</td><td>Re-copy via <code>/mybots → API Token</code>. If you ever pressed <strong>Revoke</strong>, the bot id stays the same but the key changes — an old key gives exactly this.</td></tr></tbody></table></div>
<p>Then test an actual message:</p>
<pre><code>https://api.telegram.org/bot&lt;YOUR_TOKEN&gt;/sendMessage?chat_id=&lt;YOUR_CHAT_ID&gt;&amp;text=hello</code></pre>
<ul><li><strong>Good:</strong> <code>{"ok":true,...}</code> and the message arrives on your phone.</li><li><strong><code>"chat not found"</code></strong> — you skipped Step 2, or the chat ID is wrong.</li></ul>
<p>If both of these work in a browser, they will work on the device.</p>)HELP";

static const char k_help_11[] = R"HELP(<ol><li>Open your Garage Gadget dashboard.</li><li><strong>Settings</strong> → unlock with your admin password.</li><li>Open the <strong>Notifications</strong> section:</li><li>Turn <strong>Telegram</strong> on.</li><li><strong>Bot token</strong> — paste the whole token.</li><li><strong>Chat ID</strong> — paste your numeric ID.</li><li>Choose what you want to be told about. Alarms are on by default; door open/close are off because they are noisy on a normal household door.</li><li>Press <strong>Save all settings</strong>.</li><li>Press <strong>Send test notification</strong>.</li></ol>
<p>The status line underneath reports what actually happened — <code>telegram ok</code>, or Telegram's own explanation of what went wrong.</p>
<blockquote>The UI pauses for a few seconds during the test. That is the device really contacting Telegram, not a hang.</blockquote>
<p>Stray spaces or line breaks in the token are stripped automatically, so a slightly messy paste is fine.</p>)HELP";

static const char k_help_12[] = R"HELP(<p>All of this is done by messaging <strong>@BotFather</strong>:</p>
<div class="tblWrap"><table><thead><tr><th>Task</th><th>How</th></tr></thead><tbody><tr><td>See your bots</td><td><code>/mybots</code></td></tr><tr><td>Re-copy the token</td><td><code>/mybots</code> → pick bot → <strong>API Token</strong></td></tr><tr><td>Invalidate a leaked token</td><td><code>/mybots</code> → pick bot → <strong>API Token</strong> → <strong>Revoke current token</strong>, then update the device</td></tr><tr><td>Rename it</td><td><code>/setname</code></td></tr><tr><td>Change its picture</td><td><code>/setuserpic</code></td></tr><tr><td>Delete the bot</td><td><code>/deletebot</code></td></tr></tbody></table></div>
<p><strong>Revoking breaks the device until you paste the new token in.</strong> The device will report <code>bot token rejected</code> — that is expected, not a fault.</p>
<h3>Quieting it down</h3>
<ul><li>Mute the bot chat in Telegram (bell icon) — messages still arrive, silently.</li><li>Or turn individual event types off under <strong>Settings → Configuration → Notifications</strong>.</li><li>Alarm messages are sent at high priority; routine ones are not.</li></ul>
<h3>Sending to a group instead of yourself</h3>
<p>Useful if more than one person should get garage alerts.</p>
<ol><li>Create a group and add your bot to it.</li><li>Send a message in the group.</li><li>Visit <code>https://api.telegram.org/bot&lt;YOUR_TOKEN&gt;/getUpdates</code> and find <code>"chat":{"id":-1001234567890,...}</code>.</li><li>Use that id — <strong>including the minus sign</strong> — as the chat ID on the device.</li></ol>
<p>Group IDs are negative. That is normal.</p>)HELP";

static const char k_help_13[] = R"HELP(<p>The device shows the real reason on the Settings page and in the Terminal tab. Match it here:</p>
<div class="tblWrap"><table><thead><tr><th>Message</th><th>Cause</th><th>Fix</th></tr></thead><tbody><tr><td><code>bot token rejected</code> (HTTP 401)</td><td>Token is wrong or revoked</td><td>Re-copy via <code>/mybots → API Token</code>. The device tells you which bot id and key length it is using — compare them. Note that <strong>Revoke keeps the same bot id and changes only the key</strong>, so a matching id does not mean a matching token.</td></tr><tr><td><code>not a usable token</code> (HTTP 404)</td><td>The stored value is not shaped like a token at all</td><td>Re-copy the whole thing: digits, colon, key. No brackets, no spaces.</td></tr><tr><td><code>chat not found</code></td><td>You never messaged the bot, or wrong chat ID</td><td>Do Step 2, then re-check Step 3.</td></tr><tr><td><code>Forbidden: bot was blocked by the user</code></td><td>You blocked the bot</td><td>Unblock it in Telegram.</td></tr><tr><td><code>bot token contains a space or line break</code></td><td>Bad paste</td><td>Re-copy in one action. (Newer firmware cleans this up by itself.)</td></tr><tr><td><code>bot token must look like 123456789:AA…</code></td><td>The colon or the key is missing</td><td>You probably pasted only the chat ID, or only half the token.</td></tr><tr><td><code>ESP_ERR_HTTP_CONNECT</code> / timeouts</td><td>Device cannot reach the internet</td><td>Check Wi-Fi; note that Telegram is blocked in some countries and on some corporate networks.</td></tr><tr><td><code>no transport enabled</code></td><td>Telegram switch is off, or fields blank</td><td>Turn it on and fill both fields, then <strong>Save all settings</strong> <em>before</em> testing.</td></tr></tbody></table></div>
<p>If Telegram is unreachable where you are, use <strong>ntfy</strong> instead — it is in the same Settings section, needs no account, and works the same way.</p>)HELP";

static const char k_help_14[] = R"HELP(<h3>1. Make sure you have a broker</h3>
<p>In Home Assistant: <strong>Settings → Add-ons → Add-on Store → Mosquitto broker</strong>, install, start. Then <strong>Settings → Devices &amp; Services</strong> and configure the MQTT integration if HA has not already offered it.</p>
<p>Create a user for the device: <strong>Settings → People → Users → Add</strong>, for example <code>garage</code> with its own password. Mosquitto accepts HA users by default.</p>
<h3>2. Point the device at it</h3>
<p>On the device: <strong>Settings → Configuration → Home Assistant</strong>, switch it on and fill in:</p>
<ul><li><strong>Broker address</strong> — your HA host, e.g. <code>192.168.1.10</code> or <code>homeassistant.local</code></li><li><strong>Port</strong> — <code>1883</code>, or <code>8883</code> with TLS on</li><li><strong>Username / Password</strong> — the user you just made</li></ul>
<p><strong>Save all settings.</strong> The status line underneath shows <code>Broker: connected</code> within a few seconds, and the Control tab gains a Home Assistant row.</p>
<h3>3. Nothing else</h3>
<blockquote><strong>Home Assistant cannot override the device's master switch.</strong> If door control is off on the Control tab, or the unit is still being set up, open and close commands from HA are refused and logged. Arm it on the device first — this is deliberate, so a home-automation rule cannot move a door that someone disabled on purpose.</blockquote>
<p>Home Assistant creates one device with these entities:</p>
<div class="tblWrap"><table><thead><tr><th>Entity</th><th>Type</th><th></th></tr></thead><tbody><tr><td>Garage Door</td><td><code>cover</code></td><td>open/close, with the garage device class</td></tr><tr><td></td><td></td><td><em>refused while door control is off on the device</em></td></tr><tr><td>Door sensor voltage</td><td>sensor</td><td>mV, diagnostic</td></tr><tr><td>Wi-Fi signal</td><td>sensor</td><td>dBm, diagnostic</td></tr><tr><td>Chip temperature</td><td>sensor</td><td>°C, diagnostic</td></tr><tr><td>Immediate Alarm</td><td>binary sensor</td><td><code>problem</code> class</td></tr><tr><td>Left Open Alarm</td><td>binary sensor</td><td><code>problem</code> class</td></tr><tr><td>Automatic close</td><td>switch</td><td>config</td></tr><tr><td>Immediate Alarm armed</td><td>switch</td><td>config</td></tr><tr><td>Left Open Alarm armed</td><td>switch</td><td>config</td></tr><tr><td>Auto-close delay</td><td>number</td><td>1–120 min, config</td></tr></tbody></table></div>
<p>Changes made in the web UI appear in HA and vice versa — both publish state after every change.</p>
<h3>Notifications</h3>
<p>You do not need the device's Telegram/ntfy setup if you use HA. Automate on the alarm entities instead:</p>
<pre><code>automation:
  - alias: Garage left open
    trigger:
      - platform: state
        entity_id: binary_sensor.garage_left_open_alarm
        to: "on"
    action:
      - service: notify.mobile_app_your_phone
        data:
          title: Garage left open
          message: The garage door has been open past its auto-close delay.</code></pre>
<p>The device's own notifications keep working alongside this. Running both is reasonable: HA goes down sometimes, and the device does not depend on it.</p>
<h3>Topics</h3>
<p>Everything lives under <code>garagegadget/&lt;id&gt;/</code>, where <code>&lt;id&gt;</code> is <code>garage_</code> plus the last three bytes of the MAC. Watch it all with:</p>
<pre><code>mosquitto_sub -h 192.168.1.10 -u garage -P yourpassword -t 'garagegadget/#' -v</code></pre>
<p><code>garagegadget/&lt;id&gt;/status</code> is <code>online</code> / <code>offline</code>, retained, set as the last will — which is how HA knows the device has dropped.</p>
<h3>Troubleshooting</h3>
<div class="tblWrap"><table><thead><tr><th>Status line</th><th>Meaning</th></tr></thead><tbody><tr><td><code>cannot reach broker</code></td><td>wrong address or port, or a firewall between them</td></tr><tr><td><code>broker refused the connection (code 5)</code></td><td>wrong username or password</td></tr><tr><td><code>broker refused the connection (code 4)</code></td><td>bad credentials format</td></tr><tr><td><code>connecting…</code> and staying there</td><td>broker address resolves but nothing is listening</td></tr></tbody></table></div>
<p>If entities never appear but the status says connected, check the MQTT integration is actually set up in HA (the add-on alone is not enough) and that its discovery prefix is the default <code>homeassistant</code>.</p>)HELP";

static const char k_help_15[] = R"HELP(<p>Works with the device exactly as it is; everything is on the HA side. Pays for that with polling lag, and your admin password sitting in HA's config.</p>
<p>Put the password in <code>secrets.yaml</code>:</p>
<pre><code># secrets.yaml
garage_password: your-admin-password
garage_host: 192.168.1.42</code></pre>
<p>Then in <code>configuration.yaml</code>:</p>
<pre><code>rest:
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
        value_template: "{{ value_json.leftOpenPeriods &gt; 0 }}"
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
          - service: rest_command.garage_trigger</code></pre>
<p>Add the two composed URLs to <code>secrets.yaml</code>:</p>
<pre><code>garage_status_url: http://192.168.1.42/api/status
garage_door_url: http://192.168.1.42/api/door?code=your-admin-password</code></pre>
<p>Notes:</p>
<ul><li><code>/api/status</code> needs no authentication, so the sensors work without the password. Only the trigger does.</li><li>The <code>condition</code> steps in the cover stop HA pulsing the opener when the door is already in the requested state. The opener has one button, so a redundant pulse moves the door the <em>wrong</em> way.</li><li>Give the device a <strong>DHCP reservation</strong>. These URLs hard-code its address.</li><li>Use <code>http://</code>, not <code>https://</code> — the device has no certificate.</li></ul>
<p>The password travels in a URL here, which is why MQTT is preferred. It will appear in HA logs on error.</p>)HELP";

static const char k_help_16[] = R"HELP(<p>On the device: <strong>Settings → Configuration → Dashboards</strong>, switch InfluxDB on, and fill in URL, token, organisation and bucket. Save. The Control tab's "Data backend" row should change to <code>InfluxDB + on-device</code> within a few seconds, and <strong>Settings → Configuration → Notifications</strong> is unaffected — the two are independent.</p>
<p>The token needs <strong>write</strong> access to the bucket. The dashboard needs a separate <strong>read</strong> token, which is what you give Grafana.</p>)HELP";

static const char k_help_17[] = R"HELP(<ol><li>Grafana → <strong>Dashboards → New → Import</strong></li><li>Upload <code>garage-gadget-dashboard.json</code></li><li>When prompted for <strong>InfluxDB</strong>, pick your data source (it must be configured with <strong>Flux</strong> as the query language, not InfluxQL)</li><li>Import</li></ol>
<p>Then set the <strong>Bucket</strong> variable at the top of the dashboard to your bucket name. The <strong>Device</strong> and <strong>Wi-Fi SSID</strong> dropdowns populate themselves from the data and default to <em>All</em>, which is right if you only have one unit.</p>
<h3>What's on it</h3>
<p><strong>At a glance</strong> — six tiles, always visible:</p>
<div class="tblWrap"><table><thead><tr><th>Tile</th><th>Reads</th></tr></thead><tbody><tr><td>Door</td><td>live position; green CLOSED, red OPEN</td></tr><tr><td>Last report</td><td>seconds since the device last spoke. Goes orange at 3 min, red at 10</td></tr><tr><td>Openings</td><td>how many times the door opened in the selected range</td></tr><tr><td>Immediate Alarm</td><td>the device's "should not be opening right now" alarm</td></tr><tr><td>Left Open Alarm</td><td>how many auto-close delays the door has overstayed</td></tr><tr><td>Wi-Fi signal</td><td>current RSSI, with a sparkline</td></tr></tbody></table></div>
<p><strong>Door activity</strong> — a state timeline of open/closed over time (hover a band for exact start and duration), and a bar chart of openings per day.</p>
<p><strong>Alarms and events</strong> — alarm history as stepped lines, and a filterable, colour-coded table of every event the device recorded.</p>
<p><strong>Device health</strong> — collapsed by default so it stays out of the way: Wi-Fi signal, chip temperature, free memory and uptime. Expand it when something looks wrong.</p>
<p>Every panel has a description; hover the ⓘ in its corner.</p>)HELP";

static const char k_help_18[] = R"HELP(<p><code>garage-gadget-alerts.yaml</code> has two placeholders you must replace first — your InfluxDB data source UID and your bucket name. The header comment in the file has copy-paste commands for PowerShell and bash, plus the three ways to import it.</p>
<p>The four rules:</p>
<div class="tblWrap"><table><thead><tr><th>Rule</th><th>Fires when</th><th>Severity</th></tr></thead><tbody><tr><td>Garage left open</td><td>the device's Left Open Alarm counter is non-zero</td><td>warning</td></tr><tr><td>Garage opened (Immediate Alarm)</td><td>the door opened while that alarm was armed</td><td>critical</td></tr><tr><td>Garage open for more than 15 minutes</td><td>the door sensor reads open for 15 min, <strong>regardless of whether the device's alarms are armed</strong></td><td>warning</td></tr><tr><td>Garage Gadget offline</td><td>no telemetry for 10 minutes</td><td>critical</td></tr></tbody></table></div>
<p>The last one is the reason to bother with Grafana alerting at all: a device that has lost power or Wi-Fi cannot tell you so itself, and while it is offline nothing else here is trustworthy.</p>
<p>Rules route through your <strong>default notification policy</strong>. To send one to a specific contact point, add this to the rule at the same indentation as <code>labels:</code>:</p>
<pre><code>notification_settings:
  receiver: My Contact Point</code></pre>
<p>Or route on the <code>severity: critical</code> / <code>severity: warning</code> labels the rules already carry.</p>
<h3>When an alert will not clear</h3>
<p>The alarm rules read the <strong>last</strong> <code>system_event</code> point for their alarm and stay firing until a zero arrives. The device sends that zero when the door closes — but only if the alarm is still armed at that moment. Disarm an alarm while it is firing and the clearing point is never sent, so the rule hangs firing with nothing able to resolve it.</p>
<p><strong>Settings → Configuration → Dashboards → Send alarm state report</strong> fixes that. It publishes the current state of every alarm immediately:</p>
<div class="tblWrap"><table><thead><tr><th>Alarm state</th><th>What is written</th></tr></thead><tbody><tr><td>Armed and tripped</td><td><code>1</code> (or the left-open period count)</td></tr><tr><td>Armed, not tripped</td><td><code>0</code></td></tr><tr><td>Disarmed</td><td><code>0</code> — a disarmed alarm cannot be firing</td></tr></tbody></table></div>
<p>The rule picks the new point up on its next evaluation and resolves. The button is also the quickest way to prove the whole path works end to end: press it and watch the point land in InfluxDB.</p>
<p>It reports honestly rather than optimistically — with InfluxDB turned off it says so instead of claiming to have sent something.</p>)HELP";

static const char k_help_19[] = R"HELP(<p>What the firmware writes, if you want to build your own panels. Every point is tagged <code>device=&lt;device name&gt;</code> and <code>SSID=&lt;wi-fi network&gt;</code>.</p>
<div class="tblWrap"><table><thead><tr><th>Measurement</th><th>Field</th><th>Type</th><th>Cadence</th><th>Notes</th></tr></thead><tbody><tr><td><code>garage door position</code></td><td><code>value</code></td><td>int</td><td>1 s</td><td><strong><code>1</code> = closed, <code>0</code> = open.</strong> Note the spaces in the measurement name.</td></tr><tr><td><code>wifi_status_garage_door</code></td><td><code>rssi</code></td><td>int</td><td>1 s</td><td>dBm</td></tr><tr><td><code>garage_device</code></td><td><code>temp_c</code></td><td>float</td><td>30 s</td><td>die temperature</td></tr><tr><td><code>garage_device</code></td><td><code>uptime_s</code></td><td>int</td><td>30 s</td><td>seconds since boot</td></tr><tr><td><code>garage_device</code></td><td><code>heap_free</code></td><td>int</td><td>30 s</td><td>bytes</td></tr><tr><td><code>system_event</code></td><td><code>value</code></td><td><strong>string</strong></td><td>on event</td><td>tagged <code>event=&lt;name&gt;</code></td></tr></tbody></table></div>
<p><code>system_event</code> values are strings, so numeric comparisons need <code>|&gt; toFloat()</code> after filtering to a single <code>event</code> — several event types carry non-numeric values and would fail conversion otherwise.</p>
<div class="tblWrap"><table><thead><tr><th><code>event</code> tag</th><th><code>value</code></th></tr></thead><tbody><tr><td><code>door_opened</code></td><td><code>"1"</code></td></tr><tr><td><code>door_closed</code></td><td><code>"1"</code></td></tr><tr><td><code>relay_activated</code></td><td><code>"web"</code>, <code>"auto_close"</code> or <code>"other"</code></td></tr><tr><td><code>ImmidiateAlarm</code></td><td><code>"1"</code> on raise, <code>"0"</code> on clear</td></tr><tr><td><code>LeftOpenAlarm</code></td><td>delay periods elapsed, <code>"0"</code> on clear</td></tr><tr><td><code>auto_close_flipped</code></td><td><code>"enabled"</code> / <code>"disabled"</code></td></tr><tr><td><code>auto_close_locked</code></td><td><code>"on"</code> / <code>"off"</code></td></tr></tbody></table></div>
<blockquote>The <code>ImmidiateAlarm</code> spelling is a typo that shipped in earlier firmware. It is kept deliberately: correcting it would break every existing alert rule and dashboard in the field for a cosmetic gain.</blockquote>)HELP";

static const char k_help_20[] = R"HELP(<p>The earlier hand-built dashboard hard-coded the bucket, <code>device == "ESP32"</code> and one specific SSID, and was exported in Grafana's <code>dashboard.grafana.app/v2</code> resource format — which carries a Cloud stack namespace and internal IDs, so it only re-imports into the stack it came from. This one is classic schema with <code>__inputs</code>, so it imports into any Grafana 10, 11 or 12, OSS or Cloud, and every filter is a variable. Import it alongside the old one; they have different UIDs and will not collide.</p>)HELP";

static const char k_help_21[] = R"HELP(<p><strong>The button does nothing, or says the door will not switch.</strong> Door control is off, or the device is still in setup. The <strong>Control</strong> tab shows which, right under the switch. A new or factory-reset unit ships with the relay disarmed on purpose — see Arming door control. The button is greyed out whenever the firmware would refuse, so the page and the device always agree.</p>
<p><strong>The dashboard is unreachable.</strong> Look at the LED. Magenta means it is configured but off Wi-Fi; wait — it retries on a backoff up to 5 minutes. Blue breathing means it has dropped into setup mode; join <code>GarageGadget-XXXX</code> and reconfigure.</p>
<p><strong>You changed routers, or mistyped the Wi-Fi password.</strong> Hold the recovery button for 10 seconds. The device restarts, raises its own network, and serves a page where you set the new Wi-Fi details (and a new password). Join <code>GarageGadget-XXXX</code>, password <code>garagesetup</code>, and open <code>http://192.168.4.1</code>. The hotspot shuts down again once the device is on your network.</p>
<p>The device deliberately does <strong>not</strong> raise a hotspot on its own when it cannot reach the stored network. A router reboot or a brief outage is usually temporary, and an unattended access point left broadcasting for hours is a worse outcome than a device that quietly keeps retrying. The hotspot only ever appears during first-time setup or a button-initiated recovery.</p>
<p><strong>A link opens as <code>https://</code> and fails.</strong> The device speaks plain HTTP — a LAN appliance cannot obtain a certificate for a private address. If your browser has "always use secure connections" (Chrome) or HTTPS-Only Mode (Firefox) enabled, it silently rewrites the address and then cannot connect, which looks like a broken device. Type the address with <code>http://</code> in front, or add an exception for the device. Every address the device prints, including the one in each notification, already includes the <code>http://</code> prefix for this reason.</p>
<p><strong><code>.local</code> doesn't resolve.</strong> Some Android versions and Windows setups don't do mDNS. Use the IP address — it's in the dashboard header, and in your router's client list.</p>
<p><strong>Telegram says "bot token rejected" (HTTP 401).</strong> The token is not one Telegram recognises. The device tells you the bot id and key length it is using — compare them against BotFather: send <code>/mybots</code>, pick the bot, choose <strong>API Token</strong>. Copy the whole string in one go, colon included. If in doubt, <strong>Revoke</strong> and use the fresh token. (A token pasted with a stray space or line break is cleaned up automatically, so that is no longer a cause of this.)</p>
<p><strong>Telegram says "chat not found" or "Forbidden".</strong> Open Telegram, find your bot, and send it any message. Telegram blocks bots from starting a conversation, so until you message it first it cannot reply. Then check the chat ID from @userinfobot.</p>
<p><strong>The door state is wrong, or flickers.</strong> Look at the live sensor voltage on the Control tab with the door shut and again with it open, then set <strong>Settings → Configuration → Door sensor → Door-closed threshold</strong> between the two. See §2.</p>
<p><strong>You forgot the admin password.</strong></p>
<p><strong>Hold the recovery button for 10 seconds</strong> while the device is running. The LED flashes white three times, the device restarts, and the dashboard is replaced by a recovery page asking you to set a new password. You may set new Wi-Fi credentials at the same time if the device can no longer reach your network.</p>
<p>Everything else is kept: door calibration, InfluxDB, Grafana, Telegram and ntfy recipients, and the complete event history. Only the password changes.</p>
<p>On a dev board the recovery button is <strong>BOOT</strong> (GPIO0). Note that it is checked <em>while the device runs</em>, not during power-up — GPIO0 is a strapping pin, and holding it low at reset puts the chip into USB download mode where this firmware never executes. Power the device on, wait for it to boot, then hold the button.</p>
<p>While a recovery is armed the device refuses the old password, every session, and every trusted device, and serves nothing but the recovery page. A half-finished recovery therefore cannot leave the unit unprotected.</p>
<p><em>Last resort, if the button is unreachable:</em> erase the settings partition over USB, which returns the unit to first-time setup while keeping the firmware and history.</p>
<p>Erase just the settings partition, which returns the unit to first-time setup while keeping the firmware and the whole event history:</p>
<pre><code>esptool.py --chip esp32s3 -p PORT erase_region 0x9000 0x6000</code></pre>
<p><code>0x9000</code>/<code>0x6000</code> are the offset and size of the <code>nvs</code> partition in <code>partitions.csv</code>. Power-cycle afterwards and the setup wizard runs again.</p>
<blockquote>Re-flashing the firmware alone does <strong>not</strong> help. <code>idf.py flash</code> writes the bootloader, partition table, OTA-data and application — nothing at <code>0x9000</code> — so your settings, including the password, survive it untouched.</blockquote>
<p>To wipe absolutely everything instead, including the stored history, use <code>idf.py -p PORT erase-flash</code> and then flash again.</p>
<p><strong>Factory reset.</strong> <strong>Settings → Configuration → Maintenance → Factory reset</strong> erases Wi-Fi, tokens, the password and all settings, and reboots into the wizard. The event history is kept — clear it separately if you want it gone.</p>)HELP";

static const char k_help_22[] = R"HELP(<ul><li><strong>One admin password</strong> gates the door button, all settings, history, logs and OTA. Set during setup; changeable under <strong>Settings → Configuration → Security</strong>.</li><li><strong>Token-based sessions.</strong> The password is exchanged once for a random 128-bit bearer token sent as a header. It is never put in a URL, so it cannot leak through server logs, browser history or <code>Referer</code>. Changing the password invalidates every session immediately.</li><li><strong>Secrets are write-only over the API.</strong> The settings endpoint reports whether a Wi-Fi password, InfluxDB token or Telegram token is <em>set</em>, never what it is. Leaving a secret field blank keeps the stored value.</li><li><strong>Failed logins are rate-limited</strong> by a deliberate delay.</li><li><strong>Trusted devices skip the prompt, but only until the next restart.</strong> An address that authenticates once is remembered so a household phone is not challenged on every visit. The list lives in RAM and is never written to flash, so a power cycle empties it and trust has to be re-earned; changing the password empties it too. Manage it under <strong>Settings → Configuration → Security</strong>. Note that the trust follows the IP address, so give anything on that list a DHCP reservation.</li><li><strong>Setup endpoints close after setup.</strong> While the device is unprovisioned the wizard's endpoints are open — there is no password yet. The moment setup completes they require authentication like everything else, so the recovery page is not a way around the password.</li><li><strong>The live door state is deliberately public</strong>, because the front page has to render before anyone signs in. History, logs and settings are not.</li></ul>
<p>This is a device on your home LAN. It speaks plain HTTP — a TLS certificate for <code>garage.local</code> is not something an end user can reasonably obtain. Treat LAN access as trusted, and do not port-forward it to the internet.</p>)HELP";

static const char k_help_23[] = R"HELP(<p>Requires <strong>ESP-IDF 5.3+</strong>.</p>
<pre><code>idf.py set-target esp32s3
idf.py build
idf.py -p &lt;PORT&gt; flash monitor</code></pre>
<p>The first build fetches <code>led_strip</code> and <code>mdns</code> from the component registry. There is nothing to configure before building and no secrets file to create — that is the point.</p>
<p>After changing <code>sdkconfig.defaults</code>, delete <code>sdkconfig</code> and rebuild so the new values are picked up.</p>
<p>Verified on ESP-IDF 5.3.3, xtensa-esp-elf 13.2.0: builds clean with no warnings, ~1.2 MB, 62% of the 3 MB app partition free.</p>
<p>Subsequent updates go over the air from the Update tab — no cable needed.</p>
<hr/>
<h3>Update notices</h3>
<p><strong>Settings — Firmware</strong> shows the version you are running, and can tell you when a newer one is published.</p>
<p>Switch on <em>Check GitHub for updates once a day</em> and the device asks <a href="https://github.com/amirft1374/Garage-Gadget/releases" target="_blank" rel="noopener">the releases API</a> once every 24 hours whether a newer tag exists. If it does, a notice appears at the top of the Control tab linking to the release page.</p>
<p><strong>It never downloads or installs anything.</strong> The device cannot tell a genuine release from whatever answered the request — nothing here verifies a signature — so pulling an executable off the internet and running it would be a poor trade for saving two clicks. You download the <code>.bin</code> yourself and upload it on the same page, exactly as before.</p>
<p><strong>Off by default</strong>, and the setup wizard asks rather than assuming. The check is an outbound request to a third party that reveals the device's public IP address, and a garage opener should not start talking to the internet on its own account without being asked. Nothing about the device, its configuration or its door is sent — it is a plain GET for a version number.</p>
<p>If the check fails, the Firmware tab says why rather than failing silently:</p>
<div class="tblWrap"><table><thead><tr><th>What it says</th><th>Meaning</th></tr></thead><tbody><tr><td><code>no releases published yet</code></td><td>The repository exists but has no release. Not an error</td></tr><tr><td><code>rate limited by GitHub (HTTP 403)</code></td><td>Too many checks from your IP address. It retries in 4 hours</td></tr><tr><td><code>ESP_ERR_HTTP_CONNECT</code></td><td>No route to the internet, or DNS is not resolving</td></tr><tr><td><code>up to date</code></td><td>You are running the latest published release</td></tr></tbody></table></div>
<p>Version tags are compared numerically, so <code>v10.0.0</code> is correctly newer than <code>v9.0.0</code>, and a <code>-rc</code> or <code>-beta</code> suffix is never announced as an upgrade.</p>)HELP";

static const char k_help_24[] = R"HELP(<div class="tblWrap"><table><thead><tr><th>File</th><th>Responsibility</th></tr></thead><tbody><tr><td><code>main.cpp</code></td><td>startup order, and the door state machine: open/close detection, both alarms, auto-close</td></tr><tr><td><code>app_config.h</code></td><td>compile-time factory defaults. <strong>No secrets.</strong></td></tr><tr><td><code>app_settings.c/h</code></td><td>every user setting, persisted in NVS. One field table drives load, save, JSON out and JSON in</td></tr><tr><td><code>history.c/h</code></td><td>the two-tier on-device store: flash event ring + PSRAM sample ring</td></tr><tr><td><code>notifier.c/h</code></td><td>Telegram and ntfy delivery, on a worker task</td></tr><tr><td><code>door_control.c/h</code></td><td>the only place the relay fires; logs every trigger everywhere it belongs</td></tr><tr><td><code>wifi_manager.cpp/h</code></td><td>station, setup/recovery SoftAP, scanning</td></tr><tr><td><code>web_server.cpp/h</code></td><td>routing, token auth, the JSON API, OTA</td></tr><tr><td><code>web_assets.cpp/h</code></td><td>the dashboard and wizard front-ends, as flash-resident strings</td></tr><tr><td><code>influx_uploader.cpp/h</code></td><td>optional InfluxDB telemetry</td></tr><tr><td><code>magnet.cpp</code>, <code>relay.c</code>, <code>led_rgb.c</code></td><td>hardware drivers</td></tr><tr><td><code>ha_mqtt.c/h</code></td><td>Home Assistant over MQTT: discovery, state, commands</td></tr><tr><td><code>web_help.cpp/h</code></td><td>the Help tab's content — <strong>generated</strong>, see <code>tools/gen_help.py</code></td></tr><tr><td><code>partitions.csv</code></td><td>2×3 MB OTA slots + the 1 MB <code>history</code> partition</td></tr><tr><td><code>grafana/</code></td><td>optional dashboard and alert rule templates</td></tr></tbody></table></div>
<p>Design rules worth keeping if you extend this:</p>
<ul><li><strong>Door safety does not depend on the network.</strong> The polling loop starts before Wi-Fi and keeps running whether or not anything upstream is reachable.</li><li><strong>Only <code>door_control_trigger()</code> touches the relay</strong>, so every trigger is logged identically no matter what asked for it.</li><li><strong>Adding a setting means adding one row</strong> to the table in <code>app_settings.c</code> and one input with a matching <code>data-k</code> in the HTML. Load, save, API and UI follow automatically.</li></ul>)HELP";

static const char k_help_25[] = R"HELP(<p><strong>NFC is gone.</strong> No RC522, no SPI reader, no tag payloads, no "arm write" / "cancel write" buttons, no <code>abobija/rc522</code> dependency. That becomes its own product.</p>
<p><strong>No compiled-in secrets.</strong> <code>secrets.json</code> and <code>app_secrets.c</code> are deleted. <code>main/CMakeLists.txt</code> no longer embeds anything. Everything the old build baked into the image is now entered by the owner and stored in NVS.</p>
<p><strong>Setup wizard and hardware recovery</strong> replace editing a JSON file and reflashing.</p>
<p><strong>On-device history and dashboard</strong> — the prototype's "last 5 event times" table becomes a real charted history that does not need InfluxDB or Grafana.</p>
<p><strong>Direct phone notifications</strong> — the prototype could only raise an alarm by writing to InfluxDB and having Grafana notice.</p>
<p><strong>Token auth</strong> replaces <code>?code=&lt;password&gt;</code> in URLs. <strong>Bounded Wi-Fi retry with backoff</strong> replaces a fixed 5-minute reconnect timer. <strong>A revised partition table</strong> adds the <code>history</code> partition.</p>
<p><strong>Deliberately unchanged:</strong> the InfluxDB measurement and event names, including the <code>ImmidiateAlarm</code> misspelling. Correcting it would break every existing Grafana alert and dashboard in the field for a cosmetic gain.</p>)HELP";

const help_topic_t k_help_topics[] = {
    { "Getting started", "What it does", k_help_0 },
    { "Getting started", "Hardware", k_help_1 },
    { "Getting started", "First-time setup", k_help_2 },
    { "Using it", "Using the dashboard", k_help_3 },
    { "Using it", "Where your data lives", k_help_4 },
    { "Notifications", "Notifications", k_help_5 },
    { "Notifications", "How this actually works", k_help_6 },
    { "Notifications", "Step 1 — Create the bot", k_help_7 },
    { "Notifications", "Step 2 — Start a conversation with your bot", k_help_8 },
    { "Notifications", "Step 3 — Get your chat ID", k_help_9 },
    { "Notifications", "Step 4 — Prove the token works, before involving the device", k_help_10 },
    { "Notifications", "Step 5 — Enter them on the device", k_help_11 },
    { "Notifications", "Managing the bot afterwards", k_help_12 },
    { "Notifications", "Troubleshooting", k_help_13 },
    { "Home Assistant", "Option A — MQTT (recommended)", k_help_14 },
    { "Home Assistant", "Option B — REST, no broker", k_help_15 },
    { "Dashboards", "Point the device at InfluxDB", k_help_16 },
    { "Dashboards", "Import the dashboard", k_help_17 },
    { "Dashboards", "Import the alert rules (optional)", k_help_18 },
    { "Dashboards", "Data model", k_help_19 },
    { "Dashboards", "Upgrading from the older dashboard", k_help_20 },
    { "Troubleshooting", "When something goes wrong", k_help_21 },
    { "Reference", "Security", k_help_22 },
    { "Reference", "Building and flashing", k_help_23 },
    { "Reference", "How the firmware is organised", k_help_24 },
    { "Reference", "Changes from the NFC_CHECK prototype", k_help_25 },
};

const int k_help_topic_count = 26;

const char k_help_markdown[] = R"HELPMD(# Garage Gadget - complete documentation

Generated from the firmware running on the device. Every section of the built-in Help tab is included below.

Secrets are redacted: API tokens appear as placeholders.


---

# Getting started

## What it does

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

## Hardware

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

## First-time setup

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


---

# Using it

## Using the dashboard

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

## Where your data lives

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


---

# Notifications

## Notifications

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

## How this actually works

Telegram will not let a program message you directly. Instead you create a
**bot** — an account controlled by an API token — and the device sends
messages *as that bot*.

Three things have to line up:

| Thing | What it is | Where it comes from |
|---|---|---|
| **Bot token** | The bot's password. `1234567890:AA**…` — a numeric id, a colon, then a ~35-character key. | BotFather |
| **Chat ID** | Which conversation to send to. For a private chat with yourself this is your own numeric user ID. | @userinfobot |
| **You messaging the bot first** | Telegram blocks bots from opening conversations. Until you send it something, it cannot reply. | You, in the Telegram app |

The third one is not optional and it is the step almost everyone misses.

---

## Step 1 — Create the bot

1. In Telegram, search for **@BotFather** (the one with the blue verified tick)
   and open a chat with it.
2. Send `/newbot`.
3. It asks for a **name** — the display name shown on messages. Anything:
   `My Garage`.
4. It asks for a **username** — must be unique across all of Telegram and
   must end in `bot`. For example `john_garage_9174_bot`.
5. BotFather replies with a congratulations message containing a line like:

   ```
   Use this token to access the HTTP API:
   1234567890:AA*********************************
   ```

**Copy the whole token in one action** — tap it in Telegram (it is tappable
and copies cleanly), or select from the first digit to the last character.
The colon is part of the token. Do not retype it by hand.

> Keep this token secret. Anyone who has it can post as your bot.

---

## Step 2 — Start a conversation with your bot

**Do not skip this.**

1. BotFather's message includes a `t.me/<your_bot_username>` link. Tap it.
2. Your bot's chat opens with a **START** button. Press it (or send `hi`).

Until you do this, every send attempt fails with *"chat not found"* or
*"Forbidden: bot can't initiate conversation with a user"*.

---

## Step 3 — Get your chat ID

1. Search for **@userinfobot** and open it.
2. Press **START**.
3. It replies with your details, including `Id: 123456789`.

That number is your chat ID. It is your own Telegram user ID — 9 or 10
digits, no colon.

> Do not confuse it with the number before the colon in the bot token. That
> one is the *bot's* id, not yours. They are different numbers.

---

## Step 4 — Prove the token works, before involving the device

This is the fastest way to tell a bad token from a bad chat ID. Open a
browser and visit:

```
https://api.telegram.org/botTOKEN/getMe
```

…where you replace the word `TOKEN` with your actual token. **Delete the
placeholder entirely — do not keep any angle brackets.** A finished URL looks
like this, with no space and no `/` between `bot` and the digits:

```
https://api.telegram.org/bot1234567890:AA*********************************/getMe
```

Read the reply carefully — the three outcomes mean very different things:

| Reply | Meaning | What to do |
|---|---|---|
| `"ok":true` with a `username` | The token is **valid**. Check it is the bot you expect. | Move on to the chat ID test below. |
| `"error_code":404, "Not Found"` | The **URL** is malformed — Telegram could not find a token in it at all. Almost always the placeholder or its angle brackets were left in. | Rebuild the URL exactly as above. This is *not* a token problem. |
| `"error_code":401, "Unauthorized"` | The token reached Telegram and was **rejected**. | Re-copy via `/mybots → API Token`. If you ever pressed **Revoke**, the bot id stays the same but the key changes — an old key gives exactly this. |

Then test an actual message:

```
https://api.telegram.org/bot<YOUR_TOKEN>/sendMessage?chat_id=<YOUR_CHAT_ID>&text=hello
```

- **Good:** `{"ok":true,...}` and the message arrives on your phone.
- **`"chat not found"`** — you skipped Step 2, or the chat ID is wrong.

If both of these work in a browser, they will work on the device.

---

## Step 5 — Enter them on the device

1. Open your Garage Gadget dashboard.
2. **Settings** → unlock with your admin password.
3. Open the **Notifications** section:
   - Turn **Telegram** on.
   - **Bot token** — paste the whole token.
   - **Chat ID** — paste your numeric ID.
4. Choose what you want to be told about. Alarms are on by default; door
   open/close are off because they are noisy on a normal household door.
5. Press **Save all settings**.
6. Press **Send test notification**.

The status line underneath reports what actually happened — `telegram ok`, or
Telegram's own explanation of what went wrong.

> The UI pauses for a few seconds during the test. That is the device really
> contacting Telegram, not a hang.

Stray spaces or line breaks in the token are stripped automatically, so a
slightly messy paste is fine.

---

## Managing the bot afterwards

All of this is done by messaging **@BotFather**:

| Task | How |
|---|---|
| See your bots | `/mybots` |
| Re-copy the token | `/mybots` → pick bot → **API Token** |
| Invalidate a leaked token | `/mybots` → pick bot → **API Token** → **Revoke current token**, then update the device |
| Rename it | `/setname` |
| Change its picture | `/setuserpic` |
| Delete the bot | `/deletebot` |

**Revoking breaks the device until you paste the new token in.** The device
will report `bot token rejected` — that is expected, not a fault.

### Quieting it down

- Mute the bot chat in Telegram (bell icon) — messages still arrive, silently.
- Or turn individual event types off under **Settings → Configuration → Notifications**.
- Alarm messages are sent at high priority; routine ones are not.

### Sending to a group instead of yourself

Useful if more than one person should get garage alerts.

1. Create a group and add your bot to it.
2. Send a message in the group.
3. Visit `https://api.telegram.org/bot<YOUR_TOKEN>/getUpdates` and find
   `"chat":{"id":-1001234567890,...}`.
4. Use that id — **including the minus sign** — as the chat ID on the device.

Group IDs are negative. That is normal.

---

## Troubleshooting

The device shows the real reason on the Settings page and in the Terminal
tab. Match it here:

| Message | Cause | Fix |
|---|---|---|
| `bot token rejected` (HTTP 401) | Token is wrong or revoked | Re-copy via `/mybots → API Token`. The device tells you which bot id and key length it is using — compare them. Note that **Revoke keeps the same bot id and changes only the key**, so a matching id does not mean a matching token. |
| `not a usable token` (HTTP 404) | The stored value is not shaped like a token at all | Re-copy the whole thing: digits, colon, key. No brackets, no spaces. |
| `chat not found` | You never messaged the bot, or wrong chat ID | Do Step 2, then re-check Step 3. |
| `Forbidden: bot was blocked by the user` | You blocked the bot | Unblock it in Telegram. |
| `bot token contains a space or line break` | Bad paste | Re-copy in one action. (Newer firmware cleans this up by itself.) |
| `bot token must look like 123456789:AA…` | The colon or the key is missing | You probably pasted only the chat ID, or only half the token. |
| `ESP_ERR_HTTP_CONNECT` / timeouts | Device cannot reach the internet | Check Wi-Fi; note that Telegram is blocked in some countries and on some corporate networks. |
| `no transport enabled` | Telegram switch is off, or fields blank | Turn it on and fill both fields, then **Save all settings** *before* testing. |

If Telegram is unreachable where you are, use **ntfy** instead — it is in the
same Settings section, needs no account, and works the same way.


---

# Home Assistant

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


---

# Dashboards

## Point the device at InfluxDB

On the device: **Settings → Configuration → Dashboards**, switch InfluxDB on, and fill in URL,
token, organisation and bucket. Save. The Control tab's "Data backend" row
should change to `InfluxDB + on-device` within a few seconds, and
**Settings → Configuration → Notifications** is unaffected — the two are independent.

The token needs **write** access to the bucket. The dashboard needs a
separate **read** token, which is what you give Grafana.

## Import the dashboard

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

## Import the alert rules (optional)

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


---

# Troubleshooting

## When something goes wrong

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


---

# Reference

## Security

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

## Building and flashing

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

## How the firmware is organised

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

---

## Changes from the NFC_CHECK prototype

**NFC is gone.** No RC522, no SPI reader, no tag payloads, no "arm write" /
"cancel write" buttons, no `abobija/rc522` dependency. That becomes its own
product.

**No compiled-in secrets.** `secrets.json` and `app_secrets.c` are deleted.
`main/CMakeLists.txt` no longer embeds anything. Everything the old build
baked into the image is now entered by the owner and stored in NVS.

**Setup wizard and hardware recovery** replace editing a JSON file and
reflashing.

**On-device history and dashboard** — the prototype's "last 5 event times"
table becomes a real charted history that does not need InfluxDB or Grafana.

**Direct phone notifications** — the prototype could only raise an alarm by
writing to InfluxDB and having Grafana notice.

**Token auth** replaces `?code=<password>` in URLs. **Bounded Wi-Fi retry with backoff** replaces a fixed 5-minute reconnect timer. **A revised
partition table** adds the `history` partition.

**Deliberately unchanged:** the InfluxDB measurement and event names,
including the `ImmidiateAlarm` misspelling. Correcting it would break every
existing Grafana alert and dashboard in the field for a cosmetic gain.
)HELPMD";
