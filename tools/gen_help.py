#!/usr/bin/env python3
"""Generates main/web_help.cpp from the project's Markdown documentation.

The Help tab in the web UI must not become a second, drifting copy of the
docs, so it is compiled from the same .md files that ship in the repo. Run
this after editing any of them:

    python tools/gen_help.py

Only the Markdown actually used by these documents is supported - headings,
paragraphs, lists, tables, fenced and inline code, blockquotes, bold, italic,
links and rules. It is a converter for this content, not a general engine.
"""
import html as html_lib
import io
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'main', 'web_help.cpp')

BACKTICK = chr(96)
FENCE = BACKTICK * 3

# (file, default category, {section titles to drop})
SOURCES = [
    # 'Quick start' is the repository's landing section; the device's own
    # Help covers the same ground in more depth, so it is not duplicated.
    ('README.md', None, {'Contents', 'Quick start'}),
    ('TELEGRAM.md', 'Notifications', set()),
    (os.path.join('homeassistant', 'README.md'), 'Home Assistant', set()),
    (os.path.join('grafana', 'README.md'), 'Dashboards', set()),
]

# Section title -> category. Anything unlisted falls back to the file's
# default, then to "Reference".
CATEGORY = {
    'What it does': 'Getting started',
    'Hardware': 'Getting started',
    'First-time setup': 'Getting started',
    'Using the dashboard': 'Using it',
    'Where your data lives': 'Using it',
    'Notifications': 'Notifications',
    'Security': 'Reference',
    'When something goes wrong': 'Troubleshooting',
    'Building and flashing': 'Reference',
    'How the firmware is organised': 'Reference',
}

CATEGORY_ORDER = ['Getting started', 'Using it', 'Notifications',
                  'Home Assistant', 'Dashboards', 'Troubleshooting', 'Reference']


STAR = chr(42)

# Secrets must never reach the Help tab, which is served without
# authentication and may be read over someone's shoulder. The documentation
# is written with placeholders, but a real value pasted in while debugging
# would otherwise be published to every unit's built-in manual. These
# patterns are a backstop, applied to every page after conversion.
REDACTIONS = [
    # Telegram bot token: <digits>:<key>.
    #
    # Deliberately no \b before the digits. In a URL the token follows the
    # literal "bot" (".../bot123456:AA..."), and because both are word
    # characters there is no boundary between them — an anchored pattern
    # silently skips exactly the case that matters most.
    #
    # The key part accepts asterisks too, so a partially-masked leftover is
    # caught rather than treated as already safe.
    (re.compile(r'\d{6,12}:[A-Za-z0-9_*\-]{8,}'),
     lambda m: '1234567890:AA' + STAR * 33),
    # InfluxDB / Grafana API tokens: long opaque base64-ish runs.
    (re.compile(r'\b[A-Za-z0-9_\-]{40,}={0,2}\b'),
     lambda m: m.group(0)[:4] + STAR * 16),
]


def redact(html):
    for pattern, repl in REDACTIONS:
        html = pattern.sub(repl, html)
    return html


def esc(t):
    return t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def _link(m):
    """Only external links survive as anchors.

    Every relative link in these documents points at another Markdown file or
    a repo path. Inside the Help tab those targets do not exist, so a real
    anchor would be a dead end — keep the wording, drop the link."""
    href = m.group(2)
    if href.startswith('http'):
        return '<a href="%s" target="_blank" rel="noopener">%s</a>' % (href, m.group(1))
    return m.group(1)


def inline(t):
    """Inline spans.

    Code spans are swapped for placeholders first so their contents are never
    re-processed, but the placeholders stay in the text — otherwise a link
    whose label contains code, like [`grafana/`](grafana/), would be split
    across fragments and never recognised as a link at all."""
    codes = []

    def stash(m):
        codes.append('<code>' + esc(m.group(1)) + '</code>')
        return '\x00%d\x00' % (len(codes) - 1)

    s = re.sub(BACKTICK + '([^' + BACKTICK + ']+)' + BACKTICK, stash, t)
    s = esc(s)
    s = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', _link, s)
    s = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', s)
    s = re.sub(r'(?<![*\w])\*([^*\n]+)\*(?!\*)', r'<em>\1</em>', s)
    return re.sub('\x00(\\d+)\x00', lambda m: codes[int(m.group(1))], s)


def md_to_html(md):
    lines = md.split('\n')
    html = []
    i = 0
    while i < len(lines):
        ln = lines[i]

        # fenced code
        #
        # The fence may be indented, because a code block inside a numbered
        # list is written that way. Matching only at column zero left the
        # fence markers as literal backticks in the output and dropped the
        # block out of its <pre>, which is how a token ended up rendered as
        # ordinary prose.
        if ln.lstrip().startswith(FENCE):
            indent = len(ln) - len(ln.lstrip())
            i += 1
            buf = []
            while i < len(lines) and not lines[i].lstrip().startswith(FENCE):
                # Remove the block's own indentation, keeping any deeper
                # indentation that is part of the code.
                buf.append(lines[i][indent:] if lines[i][:indent].strip() == '' else lines[i].lstrip())
                i += 1
            i += 1
            html.append('<pre><code>' + esc('\n'.join(buf)) + '</code></pre>')
            continue

        # table
        if (re.match(r'^\s*\|', ln) and i + 1 < len(lines)
                and re.match(r'^\s*\|[\s:|-]+\|\s*$', lines[i + 1])):
            def cells(row):
                return [c.strip() for c in row.strip().strip('|').split('|')]
            head = cells(ln)
            i += 2
            body = []
            while i < len(lines) and re.match(r'^\s*\|', lines[i]):
                body.append(cells(lines[i]))
                i += 1
            t = ['<div class="tblWrap"><table><thead><tr>']
            t += ['<th>' + inline(h) + '</th>' for h in head]
            t.append('</tr></thead><tbody>')
            for row in body:
                t.append('<tr>' + ''.join('<td>' + inline(c) + '</td>' for c in row) + '</tr>')
            t.append('</tbody></table></div>')
            html.append(''.join(t))
            continue

        # heading
        m = re.match(r'^(#{3,5})\s+(.*)$', ln)
        if m:
            # H2 became the page title, so the deepest heading inside a page
            # body is H3 and the levels carry over unchanged.
            lvl = min(len(m.group(1)), 5)
            html.append('<h%d>%s</h%d>' % (lvl, inline(m.group(2)), lvl))
            i += 1
            continue

        # rule
        if re.match(r'^(---+|\*\*\*+)\s*$', ln):
            html.append('<hr/>')
            i += 1
            continue

        # blockquote
        if ln.startswith('>'):
            buf = []
            while i < len(lines) and lines[i].startswith('>'):
                buf.append(lines[i].lstrip('>').strip())
                i += 1
            html.append('<blockquote>' + inline(' '.join(buf)) + '</blockquote>')
            continue

        # list
        m = re.match(r'^(\s*)([-*]|\d+\.)\s+(.*)$', ln)
        if m:
            tag = 'ul' if m.group(2) in ('-', '*') else 'ol'
            base = len(m.group(1))
            items = []
            while i < len(lines):
                mm = re.match(r'^(\s*)([-*]|\d+\.)\s+(.*)$', lines[i])
                if not mm or len(mm.group(1)) < base:
                    break
                text = [mm.group(3)]
                i += 1
                # Continuation lines belong to the item above them.
                while (i < len(lines) and lines[i].strip()
                       and not re.match(r'^(\s*)([-*]|\d+\.)\s+', lines[i])
                       and not lines[i].startswith('#')
                       and not lines[i].lstrip().startswith('|')
                       and not lines[i].lstrip().startswith(FENCE)):
                    text.append(lines[i].strip())
                    i += 1
                items.append(' '.join(text))
            html.append('<%s>%s</%s>'
                        % (tag, ''.join('<li>' + inline(x) + '</li>' for x in items), tag))
            continue

        if not ln.strip():
            i += 1
            continue

        # paragraph
        buf = []
        while (i < len(lines) and lines[i].strip()
               and not re.match(r'^(#{2,5}\s|>|\s*\||---+\s*$)', lines[i])
               and not lines[i].lstrip().startswith(FENCE)
               and not re.match(r'^(\s*)([-*]|\d+\.)\s+', lines[i])):
            buf.append(lines[i].strip())
            i += 1
        if buf:
            html.append('<p>' + inline(' '.join(buf)) + '</p>')
        else:
            i += 1
    return '\n'.join(html)


def check_help_links(topics):
    """Every "?" in the settings UI jumps to a help topic by its exact title.

    A title that no longer exists fails silently in the browser — openHelpByTitle
    finds nothing and returns, so the link just stops working and nobody
    notices for months. Catch it here instead, where the titles are generated.
    """
    ui_path = os.path.join(ROOT, 'main', 'web_assets.cpp')
    with io.open(ui_path, encoding='utf-8') as f:
        ui = f.read()

    titles = set(t['title'] for t in topics)
    broken = []
    for raw in sorted(set(re.findall(r'data-help="([^"]+)"', ui))):
        # The attribute is HTML, so entities have to be decoded before matching
        # the plain text the generator produced.
        wanted = html_lib.unescape(raw)
        if wanted not in titles:
            broken.append(raw)

    if broken:
        raise SystemExit(
            'help links point at topics that do not exist:\n  '
            + '\n  '.join(broken)
            + '\n\nAvailable titles:\n  '
            + '\n  '.join(sorted(titles)))

    print('  %d help links check out' % len(set(re.findall(r'data-help="([^"]+)"', ui))))


def main():
    topics = []
    for fname, default_cat, skip in SOURCES:
        path = os.path.join(ROOT, fname)
        if not os.path.exists(path):
            print('  ! missing ' + fname)
            continue
        md = io.open(path, encoding='utf-8').read()

        for m in re.finditer(r'^##\s+(.*?)\s*$', md, flags=re.M):
            title = re.sub(r'^\d+\.\s*', '', m.group(1)).strip()
            start = m.end()
            nxt = re.search(r'^##\s+', md[start:], flags=re.M)
            body = (md[start:start + nxt.start()] if nxt else md[start:]).strip()

            if title in skip or not body:
                continue

            html = md_to_html(body)
            # Section separators are meaningless once each section is its own
            # page, and a rule at the very end just looks like a mistake.
            html = re.sub(r'(\s*<hr/>)+\s*$', '', html)
            html = redact(html)

            topics.append({
                'title': title,
                'cat': CATEGORY.get(title, default_cat or 'Reference'),
                'html': html,
                # The original Markdown, redacted identically, so the Help tab
                # can hand the user a file to feed to an AI rather than
                # round-tripping the rendered HTML back through a converter.
                'md': redact(body),
            })

    def order(t):
        return CATEGORY_ORDER.index(t['cat']) if t['cat'] in CATEGORY_ORDER else 99
    topics.sort(key=order)

    with io.open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write('/*\n'
                ' * web_help.cpp - GENERATED FILE. Do not edit by hand.\n'
                ' *\n'
                ' * Built from the project Markdown by tools/gen_help.py so the Help\n'
                ' * tab cannot drift from the documentation that ships in the repo.\n'
                ' * Re-run that script after changing any .md file.\n'
                ' */\n\n#include "web_help.h"\n\n')

        for n, t in enumerate(topics):
            if ')HELP"' in t['html']:
                raise SystemExit('raw-string delimiter collision in ' + t['title'])
            f.write('static const char k_help_%d[] = R"HELP(%s)HELP";\n\n' % (n, t['html']))

        f.write('const help_topic_t k_help_topics[] = {\n')
        for n, t in enumerate(topics):
            f.write('    { "%s", "%s", k_help_%d },\n'
                    % (t['cat'].replace('"', ''), t['title'].replace('"', "'"), n))
        f.write('};\n\nconst int k_help_topic_count = %d;\n\n' % len(topics))

        # One concatenated Markdown document, offered as a download. Built
        # from the same redacted source as the HTML, so the two can never
        # disagree and no secret can reach one but not the other.
        doc = ['# Garage Gadget - complete documentation', '',
               'Generated from the firmware running on the device. Every section of '
               'the built-in Help tab is included below.', '',
               'Secrets are redacted: API tokens appear as placeholders.', '']
        last_cat = None
        for t in topics:
            if t['cat'] != last_cat:
                last_cat = t['cat']
                doc += ['', '---', '', '# ' + last_cat, '']
            doc += ['## ' + t['title'], '', t['md'], '']
        md = '\n'.join(doc).rstrip() + '\n'

        if ')HELPMD"' in md:
            raise SystemExit('raw-string delimiter collision in the Markdown bundle')
        f.write('const char k_help_markdown[] = R"HELPMD(%s)HELPMD";\n' % md)
        print('  markdown bundle: %d KB' % (len(md) // 1024))

    check_help_links(topics)

    print('wrote ' + os.path.relpath(OUT, ROOT))
    by_cat = {}
    for t in topics:
        by_cat.setdefault(t['cat'], []).append(t['title'])
    for c in CATEGORY_ORDER:
        if c in by_cat:
            print('  %-16s %s' % (c, ', '.join(by_cat[c])))
    total = sum(len(t['html']) for t in topics)
    print('  %d topics, %d KB of HTML' % (len(topics), total // 1024))


if __name__ == '__main__':
    main()
