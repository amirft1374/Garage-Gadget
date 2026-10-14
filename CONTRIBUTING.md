# Contributing

Issues and pull requests are welcome. Please read this first — it will save
you writing code that cannot be merged.

## Licence

This project is **not open source**. It is published for reference and
discussion under the terms in [LICENSE](LICENSE): you may read it, fork it
here, and build it for your own private use, but not redistribute it or use
it commercially without permission.

By opening a pull request you license your contribution to the project
maintainer, as set out at the end of that file. You keep the copyright in
your own work. If you would rather not grant that, open an issue describing
the change and it can be implemented separately.

## How decisions are made

This firmware runs on hardware that moves a heavy door. Every change is
reviewed by the maintainer, and **the maintainer decides what is merged and
what ships.** That is not a formality: a bug here does not crash a web page,
it closes a garage door on something.

Open an issue before writing anything substantial. A pull request that
arrives unannounced and changes how the door is triggered will be read
carefully and may still be declined — not because it is bad, but because the
safety behaviour it touches was arrived at deliberately.

Small, obvious fixes — a typo, a broken link, a compiler warning — need no
discussion. Send them.

## What makes a change easy to accept

- **One concern per pull request.** A refactor bundled with a fix is two
  reviews pretending to be one.
- **Explain the failure, not just the patch.** "Auto-close fires during setup
  because the sensor is uncalibrated" tells a reviewer far more than "add a
  guard".
- **Match the surrounding code.** Comments here explain *why* a thing is the
  way it is, not what the line does. Keep that.
- **Say what you tested on.** "Built and run on an ESP32-S3-DevKitC-1, door
  cycled 20 times" is worth more than a clean diff.

## Things that need more care than usual

- **Anything touching `relay.c` or `door_control.c`.** Exactly one function
  may energise the relay, and every trigger passes a single safety gate.
  Adding a second path around it will be rejected.
- **The settings table in `app_settings.c`.** NVS keys are limited to 15
  characters and are effectively permanent — a renamed key silently loses a
  user's setting on upgrade.
- **Documentation.** `main/web_help.cpp` is generated. Edit the Markdown and
  re-run `python tools/gen_help.py`; do not edit the generated file.

## Before you open a pull request

```bash
idf.py build                 # must compile clean, no new warnings
python tools/gen_help.py     # if you changed any .md file
```

The help generator also validates that every `?` link in the settings UI
resolves to a real help topic, and fails the build if one does not.

## Never commit

Credentials of any kind. Wi-Fi passwords, API tokens, chat IDs and your own
network's SSID are entered on the device through the setup wizard and live in
NVS. Nothing user-specific is compiled in, and nothing user-specific belongs
in a commit — git history is permanent and public.
