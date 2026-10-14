# Setting up Telegram notifications

How to get your Garage Gadget messaging your phone, and how to look after the
bot afterwards.

Total time: about five minutes. You need the Telegram app on your phone.

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
