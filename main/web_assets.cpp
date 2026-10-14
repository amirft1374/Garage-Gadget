#include "web_assets.h"

/* ═══════════════════════════════════════════════════════════════════════
 * Stylesheet — GET /app.css
 * ═══════════════════════════════════════════════════════════════════════ */

extern const char k_app_css[] = R"CSS(
:root{
  --bg:#0d1117; --panel:#161b22; --panel2:#0b0e13; --line:#272e38;
  --fg:#e6edf3; --muted:#8b949e; --accent:#2f81f7; --accent2:#1f6feb;
  --ok:#3fb950; --warn:#d29922; --bad:#f85149; --void:#05070a;
  --radius:10px;
}
@media (prefers-color-scheme:light){
  :root{
    --bg:#f4f6f9; --panel:#ffffff; --panel2:#eef1f5; --line:#d5dbe3;
    --fg:#111820; --muted:#5b6673; --void:#1c222b;
  }
}
*{box-sizing:border-box}
/* Nothing may widen the page itself; wide content scrolls inside its own
   container instead. Long tokens and URLs in the help pages were the culprit. */
html,body{overflow-x:hidden;max-width:100%}
#content{overflow-x:hidden}
body{margin:0;padding:16px;background:var(--bg);color:var(--fg);
  font:14px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
  max-width:1180px;margin-inline:auto}

/* ── shell: sidebar + content ── */
#shell{display:flex;gap:18px;align-items:flex-start}
#content{flex:1;min-width:0}
#nav{position:sticky;top:16px;width:186px;flex:none;display:flex;flex-direction:column;gap:4px;
  background:var(--panel);border:1px solid var(--line);border-radius:var(--radius);padding:12px 10px}
#navBrand{padding:4px 8px 10px;border-bottom:1px solid var(--line);margin-bottom:6px}
#navName{display:block;font-size:15px;font-weight:700;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
#navBrand small{color:var(--muted);font-size:10.5px;font-family:ui-monospace,Consolas,monospace}
.navItem{display:flex;align-items:center;gap:10px;width:100%;text-align:left;
  background:transparent;border:none;border-radius:8px;padding:10px 10px;
  color:var(--muted);font-size:13.5px;font-weight:600;cursor:pointer}
.navItem:hover{background:var(--panel2);color:var(--fg);filter:none}
.navItem.on{background:var(--accent2);color:#fff}
.navItem .ic{font-size:15px;line-height:1;width:18px;text-align:center}
#navFoot{margin-top:10px;padding-top:10px;border-top:1px solid var(--line);
  font-size:11.5px;color:var(--muted);text-align:center}
#navDoor{font-weight:700}
#navDoor.closed{color:var(--ok)} #navDoor.open{color:var(--bad)}

/* On a phone the sidebar becomes a scrollable strip along the top; a
   slide-out drawer would hide the one control people came for. */
@media (max-width:820px){
  #shell{flex-direction:column;gap:12px}
  #nav{position:static;width:100%;flex-direction:row;overflow-x:auto;padding:8px;gap:6px;align-items:center}
  #navBrand{display:none} #navFoot{display:none}
  .navItem{width:auto;flex:1;justify-content:center;white-space:nowrap;padding:9px 8px}
}

/* ── help ── */
#helpLayout{display:grid;grid-template-columns:214px minmax(0,1fr);gap:16px;align-items:start}
@media (max-width:760px){#helpLayout{grid-template-columns:1fr}}
#helpSide{position:sticky;top:16px;display:flex;flex-direction:column;gap:10px;
  max-height:calc(100vh - 32px);min-height:0}
#helpNav{background:var(--panel);border:1px solid var(--line);border-radius:var(--radius);
  padding:10px 8px;overflow:auto;min-height:0;flex:1 1 auto}
@media (max-width:760px){
  #helpSide{position:static;max-height:none}
  #helpNav{max-height:none;flex:none}
}
.helpCat{font-size:10.5px;font-weight:700;letter-spacing:.7px;text-transform:uppercase;
  color:var(--muted);padding:10px 8px 4px}
.helpCat:first-child{padding-top:2px}
.helpLink{display:block;width:100%;text-align:left;background:transparent;border:none;
  border-radius:6px;padding:7px 9px;color:var(--fg);font-size:12.8px;font-weight:500;cursor:pointer;
  line-height:1.35}
.helpLink:hover{background:var(--panel2);filter:none}
.helpLink.on{background:var(--accent2);color:#fff;font-weight:600}
.helpLoading{color:var(--muted);font-size:12px;padding:8px}
#helpDl{flex:none;padding:10px;background:var(--panel);
  border:1px solid var(--line);border-radius:var(--radius)}
#helpDlBtn{display:block;text-align:center;background:var(--panel2);border:1px solid var(--line);
  border-radius:7px;padding:8px;color:var(--fg);font-size:12.5px;font-weight:600;
  text-decoration:none;margin-bottom:6px}
#helpDlBtn:hover{background:var(--accent2);color:#fff;border-color:var(--accent2)}
#helpDl .hint{margin:0}
#helpBody{background:var(--panel);border:1px solid var(--line);border-radius:var(--radius);
  padding:20px 22px;min-height:300px;overflow-wrap:break-word}
#helpBody h2{font-size:19px;margin:0 0 12px}
#helpBody h3{font-size:15.5px;margin:22px 0 8px;padding-bottom:5px;border-bottom:1px solid var(--line)}
#helpBody h4{font-size:13.5px;margin:18px 0 6px;color:var(--fg)}
#helpBody h5{font-size:12.5px;margin:14px 0 6px;color:var(--muted)}
#helpBody p{margin:0 0 11px;font-size:13.5px;line-height:1.62}
#helpBody ul,#helpBody ol{margin:0 0 12px;padding-left:22px}
#helpBody li{margin-bottom:5px;font-size:13.5px;line-height:1.58}
#helpBody code{background:var(--panel2);border:1px solid var(--line);border-radius:4px;
  padding:1px 5px;font:12px ui-monospace,Consolas,monospace;overflow-wrap:anywhere}
#helpBody pre{background:var(--void);border:1px solid var(--line);border-radius:8px;
  padding:12px 14px;overflow-x:auto;margin:0 0 13px}
#helpBody pre code{background:none;border:none;padding:0;color:#c9d1d9;font-size:12px;line-height:1.5}
#helpBody blockquote{margin:0 0 13px;padding:9px 13px;border-left:3px solid var(--accent);
  background:var(--panel2);border-radius:0 7px 7px 0;color:var(--muted);font-size:13px}
#helpBody blockquote code{font-size:11.5px}
#helpBody hr{border:none;border-top:1px solid var(--line);margin:20px 0}
#helpBody .tblWrap{overflow-x:auto;margin:0 0 14px;border:1px solid var(--line);border-radius:8px}
#helpBody table{margin:0;font-size:12.7px}
#helpBody th,#helpBody td{padding:8px 11px;border-bottom:1px solid var(--line);vertical-align:top}
#helpBody thead th{background:var(--panel2);white-space:nowrap}
#helpBody tbody tr:last-child td{border-bottom:none}
#helpBody a{text-decoration:none;border-bottom:1px solid var(--accent)}

/* ── trusted devices ── */
.trustRow{display:flex;align-items:center;gap:8px;padding:7px 9px;background:var(--panel2);
  border:1px solid var(--line);border-radius:7px;margin-bottom:6px}
.trustRow.self{border-color:var(--accent)}
.trustRow .ip{font-family:ui-monospace,Consolas,monospace;font-size:12.5px;font-weight:600}
.trustRow .meta{font-size:11px;color:var(--muted)}
.trustRow .grow{flex:1;min-width:0}
.trustRow button{background:transparent;border:1px solid var(--line);color:var(--bad);
  padding:3px 9px;font-size:11.5px;border-radius:6px}
.selfTag{display:inline-block;margin-left:6px;padding:1px 6px;border-radius:4px;
  background:var(--accent2);color:#fff;font-size:9.5px;font-weight:700;vertical-align:middle}
#trustedList:empty::after{content:'No trusted devices — everyone is asked for the password.';
  color:var(--muted);font-size:12px}

/* ── telegram recipients ── */
.tgRow{display:flex;align-items:center;gap:8px;padding:7px 9px;background:var(--panel2);
  border:1px solid var(--line);border-radius:7px;margin-bottom:6px}
.tgRow .nick{font-weight:600;font-size:13px}
.tgRow .cid{font-family:ui-monospace,Consolas,monospace;font-size:11.5px;color:var(--muted)}
.tgRow .grow{flex:1;min-width:0}
.tgRow button{background:transparent;border:1px solid var(--line);color:var(--bad);
  padding:3px 9px;font-size:11.5px;border-radius:6px}
.tgAddRow{display:flex;gap:6px;margin-top:8px;flex-wrap:wrap}
.tgAddRow input{flex:1;min-width:130px;margin:0}
.tgAddRow button{white-space:nowrap}
#tgRecipients:empty::after{content:'No recipients yet — nobody will be notified.';
  color:var(--muted);font-size:12px}

/* ── settings sub-tabs ── */
#subBar{display:flex;gap:4px;margin:0 0 14px;background:var(--panel);border:1px solid var(--line);
  border-radius:9px;padding:4px}
.subBtn{flex:1;background:transparent;border:none;border-radius:6px;padding:8px 6px;
  color:var(--muted);font-size:12.5px;font-weight:600;cursor:pointer}
.subBtn:hover{color:var(--fg);filter:none}
.subBtn.on{background:var(--accent2);color:#fff}
.sub{display:none} .sub.on{display:block}

/* ── Configuration section navigation ── *
 * A second bar inside Configuration. Ten fieldsets on one page meant
 * scrolling past nine to reach the tenth; these wrap rather than squeeze,
 * so the bar stays readable on a phone. */
#cfgBar{display:flex;flex-wrap:wrap;gap:4px;margin:0 0 14px;background:var(--panel);
  border:1px solid var(--line);border-radius:9px;padding:4px}
.cfgBtn{background:transparent;border:none;border-radius:6px;padding:7px 11px;
  color:var(--muted);font-size:12px;font-weight:600;cursor:pointer;white-space:nowrap}
.cfgBtn:hover{color:var(--fg);filter:none}
.cfgBtn.on{background:var(--accent2);color:#fff}
.cfgPane{display:none} .cfgPane.on{display:block}

/* Save writes every field in Configuration, not just the visible section,
 * so it must not be parked at the bottom of one of them. Sticky keeps it
 * on screen wherever you are. */
#saveBar{position:sticky;bottom:0;z-index:5;display:flex;align-items:center;gap:12px;
  margin:4px 0 0;padding:12px 0;background:var(--bg);border-top:1px solid var(--line)}
h1{font-size:19px;margin:0}
h2{font-size:14px;margin:0 0 10px;color:var(--fg);font-weight:600}
a{color:var(--accent)}

/* ── header ── */
#top{display:flex;align-items:center;gap:10px;flex-wrap:wrap;margin-bottom:14px}
#top .grow{flex:1}
.pill{display:inline-flex;align-items:center;gap:6px;padding:4px 10px;border-radius:999px;
  font-size:12px;font-weight:600;background:var(--panel);border:1px solid var(--line);color:var(--muted)}
.pill .dot{width:8px;height:8px;border-radius:50%;background:var(--muted)}
.pill.ok .dot{background:var(--ok)} .pill.bad .dot{background:var(--bad)}
.pill.warn .dot{background:var(--warn)}
#netLine{font:12px/1.4 ui-monospace,Consolas,monospace;color:var(--muted)}

/* ── hero ── */
#hero{display:grid;grid-template-columns:minmax(0,1.1fr) minmax(0,1fr);gap:14px;margin-bottom:14px}
@media (max-width:640px){#hero{grid-template-columns:1fr}}
.card{background:var(--panel);border:1px solid var(--line);border-radius:var(--radius);padding:14px}
#garageCard{display:flex;align-items:center;justify-content:center;padding:8px}
#garageSvg{width:100%;height:auto;max-height:230px;display:block}
#doorPanel{transition:transform .9s cubic-bezier(.4,0,.2,1)}
#heroRight{display:flex;flex-direction:column;gap:10px}
#stateBadge{font-size:22px;font-weight:800;letter-spacing:.5px;text-align:center;
  padding:14px;border-radius:var(--radius);border:2px solid var(--ok);color:var(--ok);background:rgba(63,185,80,.09)}
#armRow{display:flex;align-items:center;justify-content:space-between;gap:10px;
  background:var(--panel2);border:1px solid var(--line);border-radius:10px;
  padding:9px 12px;margin:0 0 8px}
#armLbl{font-size:13px;font-weight:600;color:var(--fg)}
#armRow.off{border-color:var(--warn)}
#armRow.off #armLbl{color:var(--warn)}
#armNote{font-size:12px;line-height:1.45;color:var(--muted);margin:0 0 10px}
#armNote.warn{color:var(--warn)}
#armNote:empty{display:none}
#triggerBtn[disabled]{opacity:.45;cursor:not-allowed;filter:none}
#stateBadge.open{border-color:var(--bad);color:var(--bad);background:rgba(248,81,73,.09)}
#openFor{text-align:center;color:var(--muted);font-size:12px;min-height:18px}
button{font:inherit;font-weight:600;border:none;border-radius:8px;padding:10px 14px;
  background:var(--accent2);color:#fff;cursor:pointer}
button:hover{filter:brightness(1.1)} button:disabled{background:#3a3f45;color:#8b949e;cursor:not-allowed;filter:none}
button.ghost{background:transparent;border:1px solid var(--line);color:var(--fg)}
button.danger{background:var(--bad)}
button.wide{width:100%}
#triggerBtn{padding:18px;font-size:17px;background:#f59e0b;color:#241a02}
#triggerBtn:hover{background:#fbbf24}

/* ── alarm lamps ── */
#lamps{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.lamp{background:var(--panel2);border:1px solid var(--line);border-radius:8px;padding:9px 10px;
  display:flex;align-items:center;gap:8px}
.lamp .bulb{width:14px;height:14px;border-radius:50%;background:#333a44;flex:none;
  box-shadow:inset 0 0 3px rgba(0,0,0,.6)}
.lamp.armed .bulb{background:var(--warn);box-shadow:0 0 8px var(--warn)}
.lamp.tripped .bulb{background:var(--bad);box-shadow:0 0 10px var(--bad);animation:flash 1s steps(1,end) infinite}
@keyframes flash{50%{opacity:.25}}
.lamp .txt{min-width:0}
.lamp .nm{font-size:12px;font-weight:600}
.lamp .st{font-size:11px;color:var(--muted)}

/* ── tabs ── */
#tabs{display:flex;gap:6px;margin-bottom:14px;overflow-x:auto}
.tab{flex:1;min-width:88px;padding:9px 8px;background:var(--panel);border:1px solid var(--line);
  border-radius:8px;color:var(--muted);font-size:13px;font-weight:600;cursor:pointer;white-space:nowrap}
.tab.on{background:var(--accent2);color:#fff;border-color:var(--accent2)}
.pane{display:none} .pane.on{display:block}

/* ── generic rows ── */
.row{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:8px 0;
  border-bottom:1px solid var(--line)}
.row:last-child{border-bottom:none}
.row .lbl{color:var(--muted);font-size:13px}
.row .val{font-weight:600;font-variant-numeric:tabular-nums}
.hint{display:block;color:var(--muted);font-size:11.5px;margin-top:3px}
.switch{position:relative;display:inline-block;width:44px;height:24px;flex:none}
.switch input{opacity:0;width:0;height:0}
.slider{position:absolute;inset:0;background:#3a3f45;border-radius:24px;transition:.2s;cursor:pointer}
.slider:before{content:'';position:absolute;height:18px;width:18px;left:3px;bottom:3px;
  background:#fff;border-radius:50%;transition:.2s}
input:checked + .slider{background:var(--accent2)}
input:checked + .slider:before{transform:translateX(20px)}
input:disabled + .slider{opacity:.45;cursor:not-allowed}

/* ── forms ── */
.field{margin-bottom:10px}
.field label{display:block;font-size:12px;color:var(--muted);margin-bottom:4px}
input[type=text],input[type=password],input[type=number],select,textarea{
  width:100%;background:var(--panel2);border:1px solid var(--line);border-radius:7px;
  color:var(--fg);font:inherit;padding:9px 10px;outline:none}
input:focus,select:focus{border-color:var(--accent)}
input.bad{border-color:var(--bad)}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
@media (max-width:560px){.grid2{grid-template-columns:1fr}}
fieldset{border:1px solid var(--line);border-radius:var(--radius);padding:14px;margin:0 0 14px}
legend{font-size:12px;font-weight:700;color:var(--muted);text-transform:uppercase;letter-spacing:.6px;padding:0 6px}
.helpFor{display:inline-flex;align-items:center;justify-content:center;width:16px;height:16px;
  margin-left:7px;border-radius:50%;border:1px solid var(--line);background:var(--panel2);
  color:var(--muted);font-size:10.5px;font-weight:700;cursor:pointer;vertical-align:middle;
  text-transform:none;letter-spacing:0}
.helpFor:hover{background:var(--accent2);color:#fff;border-color:var(--accent2)}
.actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:10px}
.note{font-size:12px;color:var(--muted);margin:0 0 10px}
.status{font-size:12px;min-height:16px;margin-top:6px}
.status.ok{color:var(--ok)} .status.bad{color:var(--bad)} .status.busy{color:var(--muted)}

/* ── calibration ── */
#calLive{display:flex;align-items:baseline;gap:10px;justify-content:center;
  padding:14px;background:var(--panel2);border:1px solid var(--line);border-radius:8px;margin-bottom:12px}
#calLiveMv{font-size:30px;font-weight:800;font-variant-numeric:tabular-nums}
#calLiveRaw{font-size:12px;color:var(--muted);font-family:ui-monospace,Consolas,monospace}
#calBar{position:relative;height:38px;background:var(--panel2);border:1px solid var(--line);
  border-radius:8px;margin-bottom:12px;overflow:hidden}
#calBar .zone{position:absolute;top:0;bottom:0;opacity:.20}
#calBar .zone.closed{background:var(--ok)} #calBar .zone.open{background:var(--bad)}
#calBar .mark{position:absolute;top:0;bottom:0;width:2px;background:var(--muted)}
#calBar .mark.trip{background:var(--fg);width:2px}
#calBar .needle{position:absolute;top:-2px;bottom:-2px;width:3px;background:var(--accent);
  box-shadow:0 0 6px var(--accent);transition:left .3s}
#calBar .tag2{position:absolute;top:2px;font-size:9.5px;color:var(--muted);white-space:nowrap}
.calStep{display:flex;align-items:center;gap:10px;padding:9px 0;border-bottom:1px solid var(--line)}
.calStep:last-of-type{border-bottom:none}
.calStep .n{width:22px;height:22px;border-radius:50%;background:var(--line);color:var(--fg);
  display:flex;align-items:center;justify-content:center;font-size:11px;font-weight:700;flex:none}
.calStep.done .n{background:var(--ok);color:#04220b}
.calStep .txt{flex:1;min-width:0;font-size:13px}
.calStep .txt small{display:block;color:var(--muted);font-size:11.5px}
.calStep button{white-space:nowrap;padding:7px 12px;font-size:12.5px}
#calVerdict{padding:10px;border-radius:8px;font-size:12.5px;margin-top:10px}
#calVerdict.good{background:rgba(63,185,80,.13);color:var(--ok)}
#calVerdict.warn{background:rgba(210,153,34,.15);color:var(--warn)}
#calVerdict.bad{background:rgba(248,81,73,.13);color:var(--bad)}
#calVerdict.idle{background:var(--panel2);color:var(--muted)}

/* ── fixed-address fallback warning ── */
/* #top is a wrapping flex row; this takes a full line of its own rather
   than squeezing in beside the status pills. */
#netWarn{flex:0 0 100%;background:rgba(210,153,34,.14);border:1px solid var(--warn);color:var(--warn);
  border-radius:10px;padding:10px 12px;font-size:12.5px;margin:10px 0 0;line-height:1.45}
#netWarn[hidden]{display:none}

/* ── event log controls ── */
#evControls{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin:0 0 10px}
#evControls select{width:auto;min-width:190px;padding:7px 9px;font-size:12.5px}
#evControls button{padding:7px 11px;font-size:12.5px;white-space:nowrap}
#evCount{font-size:12px;color:var(--muted);margin-left:auto}
#evBody tr.tsUnknown td.ts{color:var(--warn);font-style:italic}

/* ── history ── */
#rangeBar{display:flex;gap:6px;margin-bottom:12px}
#rangeBar button{padding:6px 12px;font-size:12px;background:var(--panel);border:1px solid var(--line);color:var(--muted)}
#rangeBar button.on{background:var(--accent2);color:#fff;border-color:var(--accent2)}
.chartWrap{margin-bottom:14px}
.chartWrap .cap{font-size:12px;color:var(--muted);margin-bottom:6px;display:flex;justify-content:space-between}
svg.chart{width:100%;height:auto;display:block;background:var(--panel2);
  border:1px solid var(--line);border-radius:8px}
table{width:100%;border-collapse:collapse;font-size:12.5px}
th,td{text-align:left;padding:7px 8px;border-bottom:1px solid var(--line)}
th{color:var(--muted);font-weight:600;font-size:11.5px;text-transform:uppercase;letter-spacing:.4px}
td.ts{font-family:ui-monospace,Consolas,monospace;color:var(--muted);white-space:nowrap}
.tag{display:inline-block;padding:1px 7px;border-radius:4px;font-size:11px;font-weight:700}
.tag.open{background:rgba(248,81,73,.16);color:var(--bad)}
.tag.close{background:rgba(63,185,80,.16);color:var(--ok)}
.tag.relay{background:rgba(47,129,247,.16);color:var(--accent)}
.tag.alarm{background:rgba(210,153,34,.18);color:var(--warn)}
.tag.sys{background:rgba(139,148,158,.16);color:var(--muted)}
#tableScroll{max-height:340px;overflow:auto;border:1px solid var(--line);border-radius:8px}

/* ── terminal ── */
#logs{white-space:pre-wrap;word-break:break-word;background:var(--void);color:#c9d1d9;
  border:1px solid var(--line);padding:10px;border-radius:8px;height:60vh;min-height:280px;
  overflow:auto;font:11.5px/1.45 ui-monospace,Consolas,monospace}

/* ── OTA ── */
#otaDrop{border:2px dashed var(--line);border-radius:10px;padding:28px 18px;text-align:center;
  cursor:pointer;color:var(--muted);user-select:none}
#otaDrop.over,#otaDrop:hover{border-color:var(--accent);color:var(--fg)}
#otaDrop.has{border-color:var(--ok);color:var(--ok)}
#otaBar{height:8px;background:var(--panel2);border-radius:4px;margin:12px 0;display:none;overflow:hidden}
#otaFill{height:100%;width:0;background:var(--accent2);transition:width .15s}
#otaFill.done{background:var(--ok)}

/* ── modal ── */
#modal{display:none;position:fixed;inset:0;background:rgba(0,0,0,.6);z-index:99;
  align-items:center;justify-content:center;padding:16px}
#modal.on{display:flex}
#modalBox{background:var(--panel);border:1px solid var(--line);border-radius:12px;
  padding:20px;width:min(360px,100%);box-shadow:0 12px 40px rgba(0,0,0,.5)}
#modalTitle{font-size:15px;font-weight:700;margin-bottom:12px}
#modalMsg{font-size:13px;color:var(--muted);margin-bottom:12px}
#modalBtns{display:flex;gap:8px;margin-top:14px}
#modalBtns button{flex:1}

footer{margin-top:20px;padding-top:10px;border-top:1px solid var(--line);
  display:flex;justify-content:space-between;font-size:11px;color:var(--muted);
  font-family:ui-monospace,Consolas,monospace}
)CSS";

/* ═══════════════════════════════════════════════════════════════════════
 * Dashboard shell — GET /
 * ═══════════════════════════════════════════════════════════════════════ */

extern const char k_index_html[] = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"/>
<meta name="color-scheme" content="dark light"/>
<title>Garage Gadget</title>
<link rel="stylesheet" href="/app.css?v=__AV__"/>
</head><body>

<div id="shell">
<aside id="nav">
  <div id="navBrand"><span id="navName">Garage</span><small id="navVer"></small></div>
  <button class="navItem on" data-t="0"><span class="ic">&#127968;</span><span>Control</span></button>
  <button class="navItem" data-t="1"><span class="ic">&#128200;</span><span>History</span></button>
  <button class="navItem" data-t="2"><span class="ic">&#9881;</span><span>Settings</span></button>
  <button class="navItem" data-t="3"><span class="ic">&#128218;</span><span>Help</span></button>
  <div id="navFoot"><span id="navDoor">&mdash;</span></div>
</aside>
<main id="content">
<div id="top">
  <div class="grow">
    <h1 id="devName">Garage</h1>
    <div id="netLine">connecting&hellip;</div>
  </div>
  <span class="pill" id="wifiPill"><span class="dot"></span><span>Wi-Fi</span></span>
  <span class="pill" id="srvPill" hidden><span class="dot"></span><span>Server</span></span>
  <div id="netWarn" hidden></div>
</div>

<div class="pane on" id="p0">
<div id="hero">
  <div class="card" id="garageCard">
    <!-- Door position is shown literally: the panel slides up out of the
         opening as the real door opens, so the state reads at a glance. -->
    <svg id="garageSvg" viewBox="0 0 320 200" role="img" aria-label="Garage door">
      <defs>
        <clipPath id="openClip"><rect x="82" y="86" width="156" height="86" rx="2"/></clipPath>
        <linearGradient id="slat" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stop-color="#5b6673"/><stop offset="100%" stop-color="#404a56"/>
        </linearGradient>
      </defs>
      <rect x="0" y="172" width="320" height="28" fill="#2a313b"/>
      <path d="M32 90 L160 26 L288 90 Z" fill="#3b4553"/>
      <rect x="46" y="90" width="228" height="82" fill="#333c48"/>
      <rect x="82" y="86" width="156" height="86" rx="2" fill="var(--void)"/>
      <g clip-path="url(#openClip)">
        <g id="doorPanel">
          <rect x="82" y="86"  width="156" height="20" fill="url(#slat)" stroke="#2b333d"/>
          <rect x="82" y="107" width="156" height="20" fill="url(#slat)" stroke="#2b333d"/>
          <rect x="82" y="128" width="156" height="20" fill="url(#slat)" stroke="#2b333d"/>
          <rect x="82" y="149" width="156" height="23" fill="url(#slat)" stroke="#2b333d"/>
        </g>
      </g>
      <rect x="78" y="82" width="164" height="6" rx="3" fill="#4a5461"/>
      <circle id="ledDot" cx="256" cy="104" r="6" fill="#3fb950"/>
    </svg>
  </div>

  <div id="heroRight">
    <div id="stateBadge">&hellip;</div>
    <div id="openFor"></div>
    <div id="armRow">
      <span id="armLbl">Door control</span>
      <label class="switch"><input type="checkbox" id="armToggle"><span class="slider"></span></label>
    </div>
    <div id="armNote"></div>
    <button id="triggerBtn" class="wide">Press to Open</button>
    <div id="lamps">
      <div class="lamp" id="lampImm">
        <span class="bulb"></span>
        <span class="txt"><span class="nm">Immediate</span><br/><span class="st">off</span></span>
      </div>
      <div class="lamp" id="lampLo">
        <span class="bulb"></span>
        <span class="txt"><span class="nm">Left Open</span><br/><span class="st">off</span></span>
      </div>
    </div>
  </div>
</div>
  <div class="card">
    <h2>Automatic close</h2>
    <div class="row">
      <span class="lbl">Automatic Close
        <span class="hint">Triggers the door shut once it has been open for the delay below.</span>
      </span>
      <label class="switch"><input type="checkbox" id="acOn"><span class="slider"></span></label>
    </div>
    <div class="row">
      <span class="lbl">Delay</span>
      <span><span class="val" id="acDelay">5</span> min
        <button class="ghost" id="acEdit" style="padding:4px 10px;font-size:12px;margin-left:8px">Edit</button>
      </span>
    </div>
    <div class="row">
      <span class="lbl">Lock Automatic Close off
        <span class="hint">Hard override. While on, Automatic Close cannot be re-enabled.</span>
      </span>
      <label class="switch"><input type="checkbox" id="acLock"><span class="slider"></span></label>
    </div>
  </div>

  <div class="card" style="margin-top:12px">
    <h2>Alarms</h2>
    <div class="row">
      <span class="lbl">Immediate Alarm
        <span class="hint">Fires the moment the door is seen open.</span>
      </span>
      <label class="switch"><input type="checkbox" id="almImm"><span class="slider"></span></label>
    </div>
    <div class="row">
      <span class="lbl">Left Open Alarm
        <span class="hint">Fires once the door has been open past the auto-close delay, and again every delay after that.</span>
      </span>
      <label class="switch"><input type="checkbox" id="almLo"><span class="slider"></span></label>
    </div>
    <div class="row">
      <span class="lbl">Notifications</span>
      <span class="val" id="notifyState">&mdash;</span>
    </div>
  </div>

  <div class="card" style="margin-top:12px">
    <h2>Device</h2>
    <div class="row"><span class="lbl">Door sensor
      <span class="hint">Live voltage on the sensor pin. To set the threshold in Settings, note this value with the door shut and again with it open, then pick a number in between.</span>
    </span><span class="val" id="doorMv">&mdash;</span></div>
    <div class="row"><span class="lbl">Chip temperature</span><span class="val" id="chipTemp">&mdash;</span></div>
    <div class="row"><span class="lbl">Wi-Fi signal</span><span class="val" id="rssiVal">&mdash;</span></div>
    <div class="row"><span class="lbl">Uptime</span><span class="val" id="uptime">&mdash;</span></div>
    <div class="row"><span class="lbl">Data backend</span><span class="val" id="backend">&mdash;</span></div>
    <div class="row" id="haRow" hidden><span class="lbl">Home Assistant</span><span class="val" id="haState">&mdash;</span></div>
    <div class="row" id="grafanaRow" hidden><span class="lbl">Dashboards</span>
      <a id="grafanaLink" target="_blank" rel="noopener">Open in Grafana &rarr;</a></div>
  </div>
</div>

<div class="pane" id="p1">
  <div id="rangeBar">
    <button data-h="24" class="on">24 h</button>
    <button data-h="72">3 d</button>
    <button data-h="168">7 d</button>
    <button data-h="240">10 d</button>
  </div>

  <div class="card">
    <div class="chartWrap">
      <div class="cap"><span>Door position</span><span id="capDoor"></span></div>
      <svg class="chart" id="chDoor" viewBox="0 0 600 70" preserveAspectRatio="none"></svg>
    </div>
    <div class="chartWrap">
      <div class="cap"><span>Wi-Fi signal (dBm)</span><span id="capRssi"></span></div>
      <svg class="chart" id="chRssi" viewBox="0 0 600 110" preserveAspectRatio="none"></svg>
    </div>
    <div class="chartWrap" style="margin-bottom:0">
      <div class="cap"><span>Chip temperature (&deg;C)</span><span id="capTemp"></span></div>
      <svg class="chart" id="chTemp" viewBox="0 0 600 110" preserveAspectRatio="none"></svg>
    </div>
  </div>

  <div class="card" style="margin-top:12px">
    <h2>Event log</h2>
    <p class="note" id="storeNote">&nbsp;</p>
    <div id="evControls">
      <select id="evFilter"><option value="">All events</option></select>
      <button id="evSort" class="ghost">Newest first &darr;</button>
      <button id="evClearFilter" class="ghost" hidden>Clear filter</button>
      <span id="evCount"></span>
    </div>
    <div id="tableScroll">
      <table><thead><tr><th>When</th><th>Event</th><th>Detail</th></tr></thead>
      <tbody id="evBody"></tbody></table>
    </div>
  </div>
</div>

<div class="pane" id="p2">
  <div id="subBar">
    <button class="subBtn on" data-s="0">Configuration</button>
    <button class="subBtn" data-s="1">Terminal</button>
    <button class="subBtn" data-s="2">Firmware</button>
  </div>
<div class="sub on" id="sub0">
  <div id="settingsLocked" class="card">
    <h2>Settings are protected</h2>
    <p class="note">Enter the admin password to view and change device configuration.</p>
    <button id="unlockBtn">Unlock</button>
  </div>

  <div id="settingsBody" hidden>
    <!-- One section at a time. Every field still lives in the DOM while its
         pane is hidden, so Save writes the whole form regardless of which
         section is on screen. -->
    <div id="cfgBar">
      <button class="cfgBtn on" data-c="wifi">Wi-Fi</button>
      <button class="cfgBtn" data-c="device">Device</button>
      <button class="cfgBtn" data-c="door">Door sensor</button>
      <button class="cfgBtn" data-c="data">Dashboards</button>
      <button class="cfgBtn" data-c="ha">Home Assistant</button>
      <button class="cfgBtn" data-c="notify">Notifications</button>
      <button class="cfgBtn" data-c="security">Security</button>
      <button class="cfgBtn" data-c="maint">Maintenance</button>
    </div>

    <div class="cfgPane on" id="cfg-wifi">
    <fieldset>
      <legend>Wi-Fi<span class="helpFor" data-help="First-time setup" title="Open the help page for this section">?</span></legend>
      <div class="field">
        <label for="ssidSel">Network</label>
        <select id="ssidSel"><option value="">Scanning&hellip;</option></select>
      </div>
      <div class="field">
        <label for="s_wifi_ssid">SSID</label>
        <input type="text" id="s_wifi_ssid" data-k="wifi_ssid" autocomplete="off"/>
      </div>
      <div class="field">
        <label for="s_wifi_pass">Password <span id="wifiPassSet" class="hint" style="display:inline"></span></label>
        <input type="password" id="s_wifi_pass" data-k="wifi_pass" autocomplete="new-password" placeholder="leave blank to keep current"/>
      </div>
      <div class="actions">
        <button id="rescanBtn" class="ghost">Rescan</button>
        <button id="wifiSaveBtn">Save &amp; reconnect</button>
      </div>
      <div class="status" id="wifiStatus"></div>
    </fieldset>
    </div>

    <div class="cfgPane" id="cfg-device">
    <fieldset>
      <legend>Device<span class="helpFor" data-help="Hardware" title="Open the help page for this section">?</span></legend>
      <div class="grid2">
        <div class="field"><label for="s_device_name">Device name (also the mDNS host)</label>
          <input type="text" id="s_device_name" data-k="device_name"/></div>
        <div class="field"><label for="s_timezone">Timezone (POSIX TZ)</label>
          <input type="text" id="s_timezone" data-k="timezone"/></div>
      </div>
      <div class="field"><label for="s_static_ip">Fixed IP address on your network (optional)</label>
        <input type="text" id="s_static_ip" data-k="static_ip" placeholder="blank = get one automatically (DHCP)"/>
        <span class="hint">Leave blank unless you need the device to always be at the same
        address. A reservation in your router does the same job and survives the device
        being moved to another network, so prefer that if you can set one.
        <strong>A gateway is required</strong> if you fill this in. Takes effect after a
        restart. If the gateway does not answer, the device gives up on the fixed address
        and gets one automatically instead, so a typo costs a reboot rather than a trip to
        the garage &mdash; a warning then appears at the top of this page.</span></div>
      <div class="grid2">
        <div class="field"><label for="s_static_gw">Gateway (your router)</label>
          <input type="text" id="s_static_gw" data-k="static_gw" placeholder="192.168.2.1"/></div>
        <div class="field"><label for="s_static_mask">Subnet mask</label>
          <input type="text" id="s_static_mask" data-k="static_mask" placeholder="255.255.255.0"/></div>
        <div class="field"><label for="s_static_dns">DNS server</label>
          <input type="text" id="s_static_dns" data-k="static_dns" placeholder="blank = same as gateway"/></div>
      </div>
      <div class="field"><label for="s_web_url">Dashboard address to publish (optional)</label>
        <input type="text" id="s_web_url" data-k="web_url" placeholder="blank = use my current IP address"/>
        <span class="hint">The address put into every notification. Leave blank on a normal
        home network and the device uses whatever IP it currently has. Set it if you reach
        the device by another name &mdash; a fixed hostname, a reverse proxy, or a VPN.
        If you omit the scheme, <code>http://</code> is added, because a browser in
        HTTPS-only mode will not open a bare address.</span></div>
      <div class="field"><label for="s_relay_ms">Relay pulse length (ms)</label>
        <input type="number" id="s_relay_ms" data-k="relay_ms" min="20" max="2000" step="10"/>
        <span class="hint">How long the opener's button is held. 100&nbsp;ms suits most
        openers; raise it if yours sometimes ignores the press.
        <b>Keep it short.</b> Many openers read a multi-second hold as a request to enter
        programming or limit-setting mode rather than as a long press.</span></div>
      <div class="field"><label for="s_monitor_host">Home server to ping (optional)</label>
        <input type="text" id="s_monitor_host" data-k="monitor_host" placeholder="e.g. 192.168.1.10 — blank to disable"/></div>
    </fieldset>
    </div>

    <div class="cfgPane" id="cfg-door">
    <fieldset>
      <legend>Door sensor calibration<span class="helpFor" data-help="Hardware" title="Open the help page for this section">?</span></legend>
      <p class="note">Teach the device what your sensor actually reads in each
      position. It then treats whichever reading is <em>nearer</em> as the answer,
      so it works whether your sensor goes up or down when the door shuts.</p>

      <div id="calLive">
        <span id="calLiveMv">&mdash;</span>
        <span>mV</span>
        <span id="calLiveRaw">raw &mdash;</span>
      </div>

      <div id="calBar" title="Where the live reading sits between your two captured points">
        <div class="zone closed" id="calZoneClosed"></div>
        <div class="zone open" id="calZoneOpen"></div>
        <div class="mark" id="calMarkClosed"></div>
        <div class="mark" id="calMarkOpen"></div>
        <div class="mark trip" id="calMarkTrip"></div>
        <div class="needle" id="calNeedle"></div>
        <div class="tag2" id="calTagClosed">closed</div>
        <div class="tag2" id="calTagOpen">open</div>
      </div>

      <div class="calStep" id="calStep1">
        <span class="n">1</span>
        <span class="txt">Fully <b>close</b> the door, wait for it to settle
          <small id="calClosedVal">not captured yet</small></span>
        <button class="ghost" id="calClosedBtn">Capture closed</button>
      </div>
      <div class="calStep" id="calStep2">
        <span class="n">2</span>
        <span class="txt"><b>Open</b> the door, wait for it to settle
          <small id="calOpenVal">not captured yet</small></span>
        <button class="ghost" id="calOpenBtn">Capture open</button>
      </div>

      <div id="calVerdict" class="idle">Capture both positions to calibrate.</div>

      <div class="actions">
        <button id="calClearBtn" class="ghost">Clear calibration</button>
      </div>

      <div class="field" style="margin-top:14px">
        <label for="s_door_mv">Trip point (mV) &mdash; set automatically by calibration</label>
        <input type="number" id="s_door_mv" data-k="door_mv" min="100" max="3100" step="10"/>
        <span class="hint">Calibrating overwrites this with the midpoint of your two
        readings. You can still set it by hand if you would rather not calibrate;
        in that case higher means closed.</span>
      </div>
    </fieldset>
    </div>

    <div class="cfgPane" id="cfg-data">
    <fieldset>
      <legend>InfluxDB &mdash; optional<span class="helpFor" data-help="Point the device at InfluxDB" title="Open the help page for this section">?</span></legend>
      <p class="note">Leave off to keep everything on the device. The History tab works either way.</p>
      <div class="row"><span class="lbl">Send telemetry to InfluxDB</span>
        <label class="switch"><input type="checkbox" id="s_influx_on" data-k="influx_on"><span class="slider"></span></label></div>
      <div class="field" style="margin-top:10px"><label for="s_influx_url">URL</label>
        <input type="text" id="s_influx_url" data-k="influx_url" placeholder="https://eu-central-1-1.aws.cloud2.influxdata.com"/></div>
      <div class="field"><label for="s_influx_token">API token <span id="influxTokenSet" class="hint" style="display:inline"></span></label>
        <input type="password" id="s_influx_token" data-k="influx_token" placeholder="leave blank to keep current"/></div>
      <div class="grid2">
        <div class="field"><label for="s_influx_org">Organisation</label>
          <input type="text" id="s_influx_org" data-k="influx_org"/></div>
        <div class="field"><label for="s_influx_bucket">Bucket</label>
          <input type="text" id="s_influx_bucket" data-k="influx_bucket"/></div>
      </div>

      <div class="actions"><button id="alarmReportBtn" class="ghost">Send alarm state report</button></div>
      <div class="status" id="alarmReportStatus"></div>
      <span class="hint">Pushes the current state of every alarm to InfluxDB right now, so a
      server-side alert that is stuck firing has something to resolve against. A disarmed
      alarm reports as resolved, because a disarmed alarm cannot be firing. Use this after
      turning an alarm off while it was going &mdash; the device only sends the clearing
      point automatically if the alarm is still armed when the door shuts.</span>
    </fieldset>

    <fieldset>
      <legend>Grafana &mdash; optional<span class="helpFor" data-help="Import the dashboard" title="Open the help page for this section">?</span></legend>
      <p class="note">A link only. Grafana reads from InfluxDB; the device never talks to it.</p>
      <div class="field"><label for="s_grafana_url">Dashboard URL</label>
        <input type="text" id="s_grafana_url" data-k="grafana_url" placeholder="https://yourorg.grafana.net/d/abc123"/></div>
    </fieldset>
    </div>

    <div class="cfgPane" id="cfg-ha">
    <fieldset>
      <legend>Home Assistant &mdash; optional<span class="helpFor" data-help="Option A &mdash; MQTT (recommended)" title="Open the help page for this section">?</span></legend>
      <p class="note">Connects out to your MQTT broker and publishes discovery messages,
      so Home Assistant creates the garage door, sensors and switches by itself &mdash;
      no YAML. The device dials out, so HA never needs to reach it.</p>
      <div class="row"><span class="lbl">Publish to Home Assistant via MQTT
        <span class="hint">Needs an MQTT broker. In HA that is the Mosquitto add-on.</span></span>
        <label class="switch"><input type="checkbox" id="s_mqtt_on" data-k="mqtt_on"><span class="slider"></span></label></div>
      <div class="grid2" style="margin-top:10px">
        <div class="field"><label for="s_mqtt_host">Broker address</label>
          <input type="text" id="s_mqtt_host" data-k="mqtt_host" placeholder="192.168.1.10 or homeassistant.local"/></div>
        <div class="field"><label for="s_mqtt_port">Port</label>
          <input type="number" id="s_mqtt_port" data-k="mqtt_port" min="1" max="65535" placeholder="1883"/></div>
      </div>
      <div class="grid2">
        <div class="field"><label for="s_mqtt_user">Username</label>
          <input type="text" id="s_mqtt_user" data-k="mqtt_user"/></div>
        <div class="field"><label for="s_mqtt_pass">Password <span id="mqttPassSet" class="hint" style="display:inline"></span></label>
          <input type="password" id="s_mqtt_pass" data-k="mqtt_pass" placeholder="leave blank to keep current"/></div>
      </div>
      <div class="row"><span class="lbl">Use TLS
        <span class="hint">Only if your broker is configured for it. Port is usually 8883.</span></span>
        <label class="switch"><input type="checkbox" id="s_mqtt_tls" data-k="mqtt_tls"><span class="slider"></span></label></div>
      <div class="status" id="mqttStatus"></div>
    </fieldset>
    </div>

    <div class="cfgPane" id="cfg-notify">
    <fieldset>
      <legend>Notifications<span class="helpFor" data-help="Notifications" title="Open the help page for this section">?</span></legend>
      <p class="note">The device posts straight to these services. No server of ours is involved.</p>

      <div class="row"><span class="lbl">Telegram
        <span class="hint">1. Message &commat;BotFather, send <code>/newbot</code>, copy the token it gives you
        (it looks like <code>123456789:AAH...</code> &mdash; paste the whole thing, colon included).
        2. Message &commat;userinfobot to get your numeric chat ID.
        <b>3. Open a chat with your new bot and send it any message</b> &mdash; Telegram blocks bots
        from messaging you until you do, and this is the step that trips up almost everyone.</span></span>
        <label class="switch"><input type="checkbox" id="s_tg_on" data-k="tg_on"><span class="slider"></span></label></div>
      <div class="grid2" style="margin-top:10px">
        <div class="field"><label for="s_tg_token">Bot token <span id="tgTokenSet" class="hint" style="display:inline"></span></label>
          <input type="password" id="s_tg_token" data-k="tg_token" placeholder="leave blank to keep current"/></div>
        <div class="field">
          <label>Recipients &mdash; who gets the alerts</label>
          <div id="tgRecipients"></div>
          <input type="hidden" id="s_tg_chats" data-k="tg_chats"/>
          <div class="tgAddRow">
            <input type="text" id="tgNewId" placeholder="chat ID (yours, not the bot's)"/>
            <input type="text" id="tgNewNick" placeholder="nickname, e.g. John"/>
            <button class="ghost" id="tgAddBtn">Add</button>
          </div>
          <button class="ghost" id="tgFindBtn" style="margin-top:8px;padding:6px 11px;font-size:12.5px">Find chat IDs</button>
          <div id="tgChatList" class="status"></div>
          <span class="hint">Every recipient gets every alert you have enabled. Press
          <b>Save all settings</b> after changing the list.</span>
        </div>
      </div>

      <div class="row"><span class="lbl">ntfy
        <span class="hint">No account needed. Pick a long, unguessable topic and subscribe to it in the ntfy app.</span></span>
        <label class="switch"><input type="checkbox" id="s_ntfy_on" data-k="ntfy_on"><span class="slider"></span></label></div>
      <div class="grid2" style="margin-top:10px">
        <div class="field"><label for="s_ntfy_server">Server</label>
          <input type="text" id="s_ntfy_server" data-k="ntfy_server" placeholder="https://ntfy.sh"/></div>
        <div class="field"><label for="s_ntfy_topic">Topic</label>
          <input type="text" id="s_ntfy_topic" data-k="ntfy_topic" placeholder="garage-7f3a91c4"/></div>
      </div>

      <div class="row"><span class="lbl">Notify on alarms</span>
        <label class="switch"><input type="checkbox" id="s_notify_alarm" data-k="notify_alarm"><span class="slider"></span></label></div>
      <div class="row"><span class="lbl">Notify when the door opens</span>
        <label class="switch"><input type="checkbox" id="s_notify_open" data-k="notify_open"><span class="slider"></span></label></div>
      <div class="row"><span class="lbl">Notify when the door closes</span>
        <label class="switch"><input type="checkbox" id="s_notify_close" data-k="notify_close"><span class="slider"></span></label></div>
      <div class="row"><span class="lbl">Notify on boot and Wi-Fi recovery</span>
        <label class="switch"><input type="checkbox" id="s_notify_offline" data-k="notify_offline"><span class="slider"></span></label></div>

      <div class="actions"><button id="testNotifyBtn" class="ghost">Send test notification</button></div>
      <div class="status" id="notifyStatus"></div>
    </fieldset>
    </div>

    <div class="cfgPane" id="cfg-security">
    <fieldset>
      <legend>Trusted devices<span class="helpFor" data-help="Security" title="Open the help page for this section">?</span></legend>
      <p class="note">Devices that have already entered the password once, and are not
      asked again until the device restarts. Yours is added automatically the first
      time you sign in.</p>
      <div id="trustedList"></div>
      <div class="tgAddRow">
        <input type="text" id="trustNewIp" placeholder="192.168.1.50"/>
        <input type="text" id="trustNewLabel" placeholder="label, e.g. Kitchen tablet"/>
        <button class="ghost" id="trustAddBtn">Add</button>
      </div>
      <div class="actions">
        <button id="trustClearBtn" class="danger">Remove all</button>
      </div>
      <div class="status" id="trustStatus"></div>
      <span class="hint"><b>This list is cleared every time the device restarts</b>, so
      trust never outlives a power cycle and has to be re-earned with the password.
      It is also emptied when the admin password changes.<br/>
      <b>Trust follows the IP address, not the device.</b> While the device is running,
      if your router hands that address to something else, that device is trusted too.
      Give anything on this list a fixed address (a DHCP reservation) in your
      router.</span>
    </fieldset>

    <fieldset>
      <legend>Admin password<span class="helpFor" data-help="Security" title="Open the help page for this section">?</span></legend>
      <div class="grid2">
        <div class="field"><label for="pwCur">Current password</label>
          <input type="password" id="pwCur" autocomplete="current-password"/></div>
        <div class="field"><label for="pwNew">New password (4&ndash;32 characters)</label>
          <input type="password" id="pwNew" autocomplete="new-password"/></div>
      </div>
      <div class="actions"><button id="pwBtn">Change password</button></div>
      <div class="status" id="pwStatus"></div>
    </fieldset>
    </div>

    <div class="cfgPane" id="cfg-maint">
    <fieldset>
      <legend>Maintenance<span class="helpFor" data-help="When something goes wrong" title="Open the help page for this section">?</span></legend>
      <div class="actions">
        <button id="rebootBtn" class="ghost">Reboot</button>
        <button id="clearHistBtn" class="ghost">Clear on-device history</button>
        <button id="factoryBtn" class="danger">Factory reset</button>
      </div>
      <div class="status" id="maintStatus"></div>
    </fieldset>
    </div>

    <div id="saveBar">
        <button id="saveBtn">Save all settings</button>
        <span class="status" id="saveStatus" style="align-self:center"></span>
    </div>
  </div>
</div>
<div class="sub" id="sub1">
  <div id="logs"></div>
</div>
<div class="sub" id="sub2">
  <div class="card">
    <h2>Firmware update</h2>
    <p class="note">Upload a <code>.bin</code> built for this board. The device verifies the image before switching to it, and falls back to the running firmware if it does not validate.</p>
    <div id="otaDrop"><span id="otaLabel">Click to choose, or drop a .bin here</span></div>
    <input type="file" id="otaFile" accept=".bin" hidden/>
    <div id="otaBar"><div id="otaFill"></div></div>
    <div class="status" id="otaStatus">No file selected.</div>
    <div class="actions"><button id="otaBtn" class="wide" disabled>Upload &amp; flash</button></div>
  </div>
</div>
</div>

<div class="pane" id="p3">
  <div id="helpLayout">
    <div id="helpSide">
      <nav id="helpNav"><div class="helpLoading">Loading&hellip;</div></nav>
      <div id="helpDl">
        <a id="helpDlBtn" href="/api/help/markdown" download>&#11015; Download all as Markdown</a>
        <span class="hint">One .md file with every page. Handy for handing the whole
        manual to an AI assistant and asking it questions.</span>
      </div>
    </div>
    <article id="helpBody">
      <div id="helpWelcome">
        <h2>Instructions &amp; help</h2>
        <p class="note">Everything that ships with this product, readable on the device
        itself &mdash; including when you are locked out or offline. Pick a topic on the left.</p>
        <p class="note">These pages are generated from the product documentation, so they
        always match the firmware you are running.</p>
      </div>
    </article>
  </div>
</div>

</main>
</div>
<footer><span id="verLine"></span><span id="clock"></span></footer>

<div id="modal"><div id="modalBox">
  <div id="modalTitle">Confirm</div>
  <div id="modalMsg"></div>
  <input type="password" id="modalInput" autocomplete="current-password"/>
  <div id="modalBtns">
    <button id="modalOk">OK</button>
    <button id="modalCancel" class="ghost">Cancel</button>
  </div>
</div></div>

<script src="/app.js?v=__AV__"></script>
</body></html>
)HTML";

/* ═══════════════════════════════════════════════════════════════════════
 * Dashboard logic — GET /app.js
 * ═══════════════════════════════════════════════════════════════════════ */

extern const char k_app_js[] = R"JS(
'use strict';
const $ = (id) => document.getElementById(id);

/* The build id, recovered from this script's own ?v= URL.
 *
 * Anything the firmware can change must be fetched through a URL carrying
 * it. Relying on cache headers alone does not work: a browser that once
 * cached a response under a long max-age never asks again, so a later header
 * change cannot reach the very clients that need correcting. A versioned URL
 * is simply a different cache key, so a stale entry can never be hit. */
const ASSET_V = (() => {
  const s = document.currentScript;
  const m = s && s.src && s.src.match(/[?&]v=([^&]+)/);
  return m ? m[1] : String(Date.now());
})();

/* ── Auth ──────────────────────────────────────────────────────────────
 * A bearer token is exchanged for the admin password once, then sent as a
 * header. The password itself never appears in a URL, a server log or the
 * browser's history. */
const TOKEN_KEY = 'gg_token';
let token = sessionStorage.getItem(TOKEN_KEY) || '';

async function api(path, opts, isRetry) {
  opts = opts || {};
  const headers = opts.headers || {};
  if (token) headers['X-Auth-Token'] = token;
  if (opts.json !== undefined) {
    headers['Content-Type'] = 'application/json';
    opts.body = JSON.stringify(opts.json);
  }
  const r = await fetch(path, { method: opts.method || 'GET', headers, body: opts.body });

  if (r.status === 401) {
    /* Sessions live in RAM on the device, so every reboot — including a
       firmware update — invalidates them while this page is still open.
       Re-authenticate and replay the request once, so an expired session
       costs the user a password prompt instead of a dead-end error. */
    token = '';
    sessionStorage.removeItem(TOKEN_KEY);
    if (!isRetry && await ensureAuth()) {
      return api(path, opts, true);
    }
    throw new Error('unauthorized');
  }
  return r;
}

async function apiJson(path, opts) {
  const r = await api(path, opts);
  if (!r.ok) throw new Error('HTTP ' + r.status);
  return r.json();
}

/* Ensures we hold a valid token, prompting for the password if not.
 * Returns true when authenticated. */
let isTrusted = false;

async function ensureAuth() {
  if (token) return true;
  /* The server already accepts this address, so a prompt would be theatre. */
  if (isTrusted) return true;
  const pw = await askPassword('Admin password', 'Required to change device settings.');
  if (!pw) return false;
  const r = await fetch('/api/login', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ password: pw })
  });
  if (!r.ok) { await askAlert('Wrong password', 'That password was not accepted.'); return false; }
  const j = await r.json();
  token = j.token;
  sessionStorage.setItem(TOKEN_KEY, token);
  return true;
}

/* ── Modal ─────────────────────────────────────────────────────────── */
function modal(opts) {
  return new Promise((resolve) => {
    const m = $('modal'), inp = $('modalInput'), ok = $('modalOk'), cancel = $('modalCancel');
    $('modalTitle').textContent = opts.title || 'Confirm';
    $('modalMsg').textContent = opts.msg || '';
    inp.hidden = !opts.input;
    inp.type = opts.inputType || 'password';
    inp.value = opts.value || '';
    cancel.hidden = !!opts.alert;
    ok.textContent = opts.okText || 'OK';
    ok.className = opts.danger ? 'danger' : '';
    m.classList.add('on');
    if (opts.input) setTimeout(() => { inp.focus(); inp.select(); }, 50);

    function done(v) {
      m.classList.remove('on');
      ok.onclick = cancel.onclick = inp.onkeydown = null;
      ok.className = '';
      resolve(v);
    }
    ok.onclick = () => done(opts.input ? (inp.value || null) : true);
    cancel.onclick = () => done(null);
    inp.onkeydown = (e) => {
      if (e.key === 'Enter') done(inp.value || null);
      else if (e.key === 'Escape') done(null);
    };
  });
}
const askPassword = (t, m) => modal({ title: t, msg: m, input: true });
const askConfirm  = (t, m, danger) => modal({ title: t, msg: m, danger, okText: danger ? 'Yes, do it' : 'Confirm' });
const askAlert    = (t, m) => modal({ title: t, msg: m, alert: true });
const askNumber   = (t, m, v) => modal({ title: t, msg: m, input: true, inputType: 'number', value: v });

/* ── Tabs ──────────────────────────────────────────────────────────── */
document.querySelectorAll('.navItem').forEach((b) => {
  b.onclick = async () => {
    const n = +b.dataset.t;
    /* History and Settings reveal when the house is empty and how the device
       is configured, so both sit behind the password. Control stays open —
       the front page has to render before anyone signs in. */
    if (n !== 0 && n !== 3 && !await ensureAuth()) return;
    document.querySelectorAll('.navItem').forEach((x, i) => x.classList.toggle('on', i === n));
    document.querySelectorAll('.pane').forEach((x, i) => x.classList.toggle('on', i === n));
    window.scrollTo(0, 0);
    if (n === 1) loadHistory();
    if (n === 2) loadSettings();
    if (n === 3) loadHelp();
  };
});

document.querySelectorAll('.cfgBtn').forEach((b) => {
  b.onclick = () => {
    const id = 'cfg-' + b.dataset.c;
    document.querySelectorAll('.cfgBtn').forEach((x) => x.classList.toggle('on', x === b));
    document.querySelectorAll('.cfgPane').forEach((pane) => pane.classList.toggle('on', pane.id === id));
    /* Jump back to the top: switching section while scrolled halfway down a
       long one otherwise lands you in the middle of the new one. */
    document.getElementById('content').scrollIntoView({ block: 'start' });
  };
});

document.querySelectorAll('.subBtn').forEach((b) => {
  b.onclick = () => {
    const n = +b.dataset.s;
    document.querySelectorAll('.subBtn').forEach((x, i) => x.classList.toggle('on', i === n));
    document.querySelectorAll('.sub').forEach((x, i) => x.classList.toggle('on', i === n));
  };
});

/* ── Formatting ────────────────────────────────────────────────────── */
function fmtTime(ts) {
  if (!ts) return 'before clock sync';
  return new Date(ts * 1000).toLocaleString([], {
    month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit', second: '2-digit'
  });
}
function fmtDur(sec) {
  if (sec < 60) return sec + 's';
  if (sec < 3600) return Math.floor(sec / 60) + 'm ' + (sec % 60) + 's';
  if (sec < 86400) return Math.floor(sec / 3600) + 'h ' + Math.floor((sec % 3600) / 60) + 'm';
  return Math.floor(sec / 86400) + 'd ' + Math.floor((sec % 86400) / 3600) + 'h';
}

/* ── Live status ───────────────────────────────────────────────────── */
let lastStatus = {};

async function refreshStatus() {
  let j;
  try { j = await (await fetch('/api/status')).json(); } catch (e) { return; }
  lastStatus = j;

  const open = !j.doorClosed;
  $('stateBadge').textContent = open ? 'OPEN' : 'CLOSED';
  $('stateBadge').className = open ? 'open' : '';
  $('triggerBtn').textContent = open ? 'Press to Close' : 'Press to Open';

  /* The relay master switch. `controlBlocked` is the device's own reason and
     is authoritative — it also covers setup mode, where the switch itself
     cannot help. Disabling the button is a courtesy; the firmware refuses
     regardless of what this page does. */
  const blocked = j.controlBlocked || '';
  $('armToggle').checked = !!j.controlEnabled;
  $('armRow').classList.toggle('off', !j.controlEnabled);
  $('triggerBtn').disabled = !!blocked;
  $('armNote').textContent = blocked
    ? blocked + ' — the relay will not switch.'
    : (j.controlEnabled ? '' : 'Switch this on to let the door be moved.');
  $('armNote').classList.toggle('warn', !!blocked);

  /* Slide the SVG panel fully out of the opening when the door is open. */
  $('doorPanel').setAttribute('transform', open ? 'translate(0,-88)' : 'translate(0,0)');
  $('ledDot').setAttribute('fill', open ? '#f85149' : '#3fb950');

  $('openFor').textContent = open && j.openSeconds
    ? 'Open for ' + fmtDur(j.openSeconds)
    : '';

  const wp = $('wifiPill');
  wp.className = 'pill ' + (j.wifiConnected ? 'ok' : 'bad');
  wp.lastElementChild.textContent = j.wifiConnected ? (j.rssi + ' dBm') : 'Offline';

  if (j.monitorEnabled) {
    const sp = $('srvPill');
    sp.hidden = false;
    sp.className = 'pill ' + (j.serverKnown ? (j.serverOnline ? 'ok' : 'bad') : 'warn');
    sp.lastElementChild.textContent = 'Server ' +
      (j.serverKnown ? (j.serverOnline ? 'up' : 'down') : '…');
  } else {
    $('srvPill').hidden = true;
  }

  $('acOn').checked = !!j.autoClose;
  $('acOn').disabled = !!j.autoCloseLocked;
  $('acLock').checked = !!j.autoCloseLocked;
  $('acDelay').textContent = j.autoCloseDelayMin;
  $('almImm').checked = !!j.alarmImmediate;
  $('almLo').checked = !!j.alarmLeftOpen;

  lamp('lampImm', j.alarmImmediate, j.immediateTripped, j.immediateTripped ? 'TRIPPED' : (j.alarmImmediate ? 'armed' : 'off'));
  lamp('lampLo', j.alarmLeftOpen, j.leftOpenPeriods > 0,
    j.leftOpenPeriods > 0 ? ('TRIPPED ×' + j.leftOpenPeriods) : (j.alarmLeftOpen ? 'armed' : 'off'));

  /* Sensor readout, shown next to the chip temperature: the voltage the
     door sensor is producing right now, plus the raw ADC count behind it. */
  const nearEdge = Math.abs(j.doorMv - j.doorThresholdMv) < 150;
  $('doorMv').innerHTML =
    '<span style="color:' + (nearEdge ? 'var(--warn)' : 'var(--fg)') + '">' +
      j.doorMv + ' mV</span>' +
    '<span style="color:var(--muted);font-weight:400"> · raw ' + j.doorRaw +
      ' · trip ' + j.doorThresholdMv + (j.doorCalibrated ? '' : ' · uncal') + '</span>';
  updateCalPanel(j);
  $('chipTemp').textContent = j.tempKnown ? j.tempC.toFixed(1) + ' °C' : '—';
  $('rssiVal').textContent = j.wifiConnected ? j.rssi + ' dBm' : '—';
  $('uptime').textContent = fmtDur(j.uptimeSec);
  $('backend').textContent = j.influxActive ? 'InfluxDB + on-device' : 'on-device only';
  $('notifyState').textContent = j.notifyEnabled ? (j.notifyLast || 'ready') : 'not configured';
  const ms = $('mqttStatus');
  if (ms) setStatus('mqttStatus', 'Broker: ' + (j.mqtt || 'disabled'),
    /^connected/.test(j.mqtt || '') ? 'ok'
      : /^(disabled|connecting)/.test(j.mqtt || '') ? 'busy' : 'bad');
  const hr = $('haRow');
  if (hr) {
    hr.hidden = /^disabled/.test(j.mqtt || '');
    $('haState').textContent = j.mqtt || '';
  }
  isTrusted = !!j.trusted;
  $('devName').textContent = j.deviceName || 'Garage';
  $('navName').textContent = j.deviceName || 'Garage';
  $('navVer').textContent = 'v' + j.version;
  const nd = $('navDoor');
  nd.textContent = j.doorClosed ? 'CLOSED' : 'OPEN';
  nd.className = j.doorClosed ? 'closed' : 'open';
  $('verLine').textContent = 'v' + j.version;
  $('clock').textContent = new Date().toLocaleTimeString();

  if (j.grafanaUrl) {
    $('grafanaRow').hidden = false;
    $('grafanaLink').href = j.grafanaUrl;
  } else {
    $('grafanaRow').hidden = true;
  }
}

function lamp(id, armed, tripped, text) {
  const el = $(id);
  el.className = 'lamp' + (tripped ? ' tripped' : (armed ? ' armed' : ''));
  el.querySelector('.st').textContent = text;
}

async function refreshNetwork() {
  try {
    const n = await (await fetch('/api/network')).json();
    /* Always show the scheme. The device is HTTP-only, and a browser with
       "always use secure connections" turned on will upgrade a bare host and
       then fail — which reads as a broken device, not a browser setting. */
    const port = (n.port === 80) ? '' : (':' + n.port);
    $('netLine').textContent = n.ip
      ? ('http://' + n.ip + port + '  ·  http://' + n.mdns + '.local' + port)
      : (n.apIp ? ('setup mode · ' + n.apSsid + ' · http://' + n.apIp) : 'offline');

    /* The device is not where its own settings say it should be. Say so
       plainly: this is the sort of thing people otherwise lose an hour to. */
    const warn = $('netWarn');
    if (n.staticRevert) {
      warn.textContent = 'The fixed address ' + (n.staticIp || '') + ' could not be used — '
        + 'its gateway did not answer, so the device is on ' + (n.ip || 'an automatic address')
        + ' instead. Check the gateway and subnet mask in Settings, then restart. '
        + 'Clear the fixed address to stay on automatic.';
      warn.hidden = false;
    } else {
      warn.hidden = true;
    }
  } catch (e) { /* transient during reconnect */ }
}

async function refreshLogs() {
  if (!$('sub1').classList.contains('on') || !token) return;
  try {
    const r = await api('/api/logs');
    if (!r.ok) return;
    const t = await r.text();
    const el = $('logs');
    const stick = el.scrollTop + el.clientHeight >= el.scrollHeight - 30;
    el.textContent = t;
    if (stick) el.scrollTop = el.scrollHeight;
  } catch (e) { /* ignore */ }
}

/* ── Actions ───────────────────────────────────────────────────────── */
async function guarded(fn, confirmTitle, confirmMsg) {
  if (!await ensureAuth()) return false;
  if (confirmTitle && !await askConfirm(confirmTitle, confirmMsg)) return false;
  try { await fn(); return true; }
  catch (e) {
    if (String(e.message) === 'unauthorized') { await askAlert('Session expired', 'Please authenticate again.'); }
    else { await askAlert('Action failed', String(e.message)); }
    return false;
  }
}

$('armToggle').onchange = async () => {
  const on = $('armToggle').checked;
  if (on && !await askConfirm('Allow the door to be moved?',
      'The relay will be able to fire — from this page, from auto-close and from Home Assistant. Make sure nobody is at the door.')) {
    $('armToggle').checked = false;
    return;
  }
  try {
    const r = await api('/api/control-enable?enabled=' + (on ? '1' : '0'), { method: 'POST' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
  } catch (e) {
    $('armToggle').checked = !on;
  }
  await refreshStatus();
};

$('triggerBtn').onclick = async () => {
  const open = !lastStatus.doorClosed;
  await guarded(async () => {
    const r = await api('/api/door', { method: 'POST' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    await refreshStatus();
  }, 'Move the garage door?', 'This will ' + (open ? 'close' : 'open') + ' the door now.');
};

$('acOn').onchange = async () => {
  const v = $('acOn').checked;
  await guarded(() => api('/api/auto-close?enabled=' + (v ? 1 : 0), { method: 'POST' }));
  /* Re-read either way: on failure this snaps the toggle back to the truth. */
  await refreshStatus();
};

$('acLock').onchange = async () => {
  const v = $('acLock').checked;
  await guarded(() => api('/api/auto-close-lock?enabled=' + (v ? 1 : 0), { method: 'POST' }),
    v ? 'Lock Automatic Close off?' : 'Unlock Automatic Close?',
    v ? 'The door will never close itself until you unlock this.' : 'Automatic Close will be re-enabled.');
  await refreshStatus();
};

$('acEdit').onclick = async () => {
  if (!await ensureAuth()) return;
  const v = await askNumber('Auto-close delay', 'Minutes the door may stay open (1–120).', $('acDelay').textContent);
  if (!v) return;
  const n = parseInt(v, 10);
  if (isNaN(n) || n < 1 || n > 120) { await askAlert('Out of range', 'Enter a whole number from 1 to 120.'); return; }
  await guarded(() => api('/api/auto-close-delay?minutes=' + n, { method: 'POST' }));
  await refreshStatus();
};

$('almImm').onchange = async () => {
  await guarded(() => api('/api/alarm?alarm=immediate&engaged=' + ($('almImm').checked ? 1 : 0), { method: 'POST' }));
  await refreshStatus();
};
$('almLo').onchange = async () => {
  await guarded(() => api('/api/alarm?alarm=left_open&engaged=' + ($('almLo').checked ? 1 : 0), { method: 'POST' }));
  await refreshStatus();
};

/* ── History ───────────────────────────────────────────────────────── */
let histHours = 24;

document.querySelectorAll('#rangeBar button').forEach((b) => {
  b.onclick = () => {
    document.querySelectorAll('#rangeBar button').forEach((x) => x.classList.remove('on'));
    b.classList.add('on');
    histHours = +b.dataset.h;
    loadHistory();
  };
});

async function loadHistory() {
  if (!await ensureAuth()) return;
  const since = Math.floor(Date.now() / 1000) - histHours * 3600;
  try {
    const [s, e, st] = await Promise.all([
      apiJson('/api/history/samples?points=400&since=' + since),
      apiJson('/api/history/events?limit=200&since=' + since),
      apiJson('/api/history/stats')
    ]);
    drawDoor(s.s);
    drawLine('chRssi', s.s, 2, (v) => v, 'capRssi', ' dBm');
    drawLine('chTemp', s.s, 3, (v) => v / 10, 'capTemp', ' °C');
    $('capDoor').textContent = s.n + ' samples';
    fillEvents(e.ev);
    $('storeNote').textContent =
      st.eventCount + ' of ' + st.eventCapacity + ' event slots used in flash · ' +
      st.sampleCount + ' of ' + st.sampleCapacity + ' samples in ' +
      (st.samplesInPsram ? 'PSRAM' : 'RAM') +
      (st.eventOldest ? ' · oldest event ' + fmtTime(st.eventOldest) : '');
  } catch (err) {
    $('storeNote').textContent = 'Could not load history: ' + err.message;
  }
}

const SVGNS = 'http://www.w3.org/2000/svg';
function el(tag, attrs) {
  const n = document.createElementNS(SVGNS, tag);
  for (const k in attrs) n.setAttribute(k, attrs[k]);
  return n;
}

function drawDoor(pts) {
  const svg = $('chDoor');
  svg.textContent = '';
  if (!pts || !pts.length) return;
  const t0 = pts[0][0], t1 = pts[pts.length - 1][0] || (t0 + 1);
  const span = Math.max(1, t1 - t0);
  for (let i = 0; i < pts.length; i++) {
    const x = ((pts[i][0] - t0) / span) * 600;
    const xn = (i + 1 < pts.length) ? ((pts[i + 1][0] - t0) / span) * 600 : 600;
    svg.appendChild(el('rect', {
      x, y: 12, width: Math.max(0.7, xn - x), height: 34,
      fill: pts[i][1] ? '#3fb950' : '#f85149'
    }));
  }
  addAxis(svg, t0, t1, 60);
}

function drawLine(id, pts, idx, map, capId, unit) {
  const svg = $(id);
  svg.textContent = '';
  if (!pts || !pts.length) { $(capId).textContent = 'no data yet'; return; }

  const vals = pts.map((p) => map(p[idx]));
  let lo = Math.min(...vals), hi = Math.max(...vals);
  if (hi - lo < 1) { hi = lo + 1; }
  const pad = (hi - lo) * 0.12; lo -= pad; hi += pad;

  const t0 = pts[0][0], t1 = pts[pts.length - 1][0] || (t0 + 1);
  const span = Math.max(1, t1 - t0);
  const X = (t) => ((t - t0) / span) * 600;
  const Y = (v) => 88 - ((v - lo) / (hi - lo)) * 76;

  /* Horizontal guides at the two extremes plus the midpoint. */
  [lo, (lo + hi) / 2, hi].forEach((g) => {
    svg.appendChild(el('line', { x1: 0, y1: Y(g), x2: 600, y2: Y(g), stroke: '#272e38', 'stroke-width': 1 }));
    const tx = el('text', { x: 4, y: Y(g) - 3, fill: '#8b949e', 'font-size': 9 });
    tx.textContent = g.toFixed(1);
    svg.appendChild(tx);
  });

  let d = '';
  pts.forEach((p, i) => { d += (i ? 'L' : 'M') + X(p[0]).toFixed(1) + ' ' + Y(map(p[idx])).toFixed(1); });
  svg.appendChild(el('path', { d, fill: 'none', stroke: '#2f81f7', 'stroke-width': 1.6, 'stroke-linejoin': 'round' }));

  addAxis(svg, t0, t1, 100);
  const last = vals[vals.length - 1];
  $(capId).textContent = 'now ' + last.toFixed(1) + unit +
    '  ·  min ' + Math.min(...vals).toFixed(1) + '  ·  max ' + Math.max(...vals).toFixed(1);
}

function addAxis(svg, t0, t1, y) {
  for (let i = 0; i <= 4; i++) {
    const t = t0 + ((t1 - t0) * i) / 4;
    const x = Math.min(575, Math.max(2, (i / 4) * 600 - (i === 0 ? 0 : 22)));
    const tx = el('text', { x, y: y + 8, fill: '#8b949e', 'font-size': 9 });
    tx.textContent = new Date(t * 1000).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    svg.appendChild(tx);
  }
}

const EV_NAMES = ['Booted', 'Door opened', 'Door closed', 'Door triggered',
  'Immediate Alarm', 'Left Open Alarm', 'Wi-Fi lost', 'Wi-Fi up',
  'Auto-close changed', 'Settings changed', 'Notification failed'];
const EV_CLASS = ['sys', 'open', 'close', 'relay', 'alarm', 'alarm', 'sys', 'sys', 'sys', 'sys', 'alarm'];

function evDetail(type, v) {
  switch (type) {
    case 3: return ['from the web UI', 'auto-close', 'other'][v] || '';
    case 4: return v ? 'raised' : 'cleared';
    case 5: return v ? ('open past ' + v + ' delay period' + (v > 1 ? 's' : '')) : 'cleared';
    case 6: return 'reason ' + v;
    case 8: return v ? 'enabled' : 'disabled';
    case 9: return v === 2 ? 'factory reset' : 'changed';
    default: return '';
  }
}

/* The device returns events newest-first in write order, so position is the
   authoritative chronology. Sorting on the timestamp would scatter the
   records written before the clock synced, which carry ts=0 — reversing the
   array keeps those in their true place. */
let evData = [];
let evNewestFirst = true;

function fillEvents(ev) {
  evData = ev || [];
  refreshEventFilter();
  renderEvents();
}

function refreshEventFilter() {
  const sel = $('evFilter');
  const keep = sel.value;

  const counts = new Map();
  evData.forEach((r) => counts.set(r[1], (counts.get(r[1]) || 0) + 1));

  sel.textContent = '';
  const all = document.createElement('option');
  all.value = '';
  all.textContent = 'All events (' + evData.length + ')';
  sel.appendChild(all);

  [...counts.keys()]
    .sort((a, b) => (EV_NAMES[a] || '').localeCompare(EV_NAMES[b] || ''))
    .forEach((type) => {
      const o = document.createElement('option');
      o.value = String(type);
      o.textContent = (EV_NAMES[type] || ('type ' + type)) + ' (' + counts.get(type) + ')';
      sel.appendChild(o);
    });

  /* Keep the user's choice across the 1.5 s refresh, unless that event type
     has dropped out of the range entirely. */
  sel.value = [...sel.options].some((o) => o.value === keep) ? keep : '';
}

function renderEvents() {
  const b = $('evBody');
  const pick = $('evFilter').value;
  let rows = (pick === '') ? evData : evData.filter((r) => String(r[1]) === pick);
  if (!evNewestFirst) rows = rows.slice().reverse();

  $('evClearFilter').hidden = (pick === '');
  $('evCount').textContent = pick === ''
    ? (rows.length + (rows.length === 1 ? ' event' : ' events'))
    : (rows.length + ' of ' + evData.length + ' shown');

  b.textContent = '';
  if (!rows.length) {
    const tr = document.createElement('tr');
    tr.innerHTML = '<td colspan="3" style="color:var(--muted)">'
      + (evData.length ? 'No events of that type in this range.'
                       : 'No events recorded in this range.') + '</td>';
    b.appendChild(tr);
    return;
  }

  for (const [ts, type, val] of rows) {
    const tr = document.createElement('tr');
    if (!ts) tr.className = 'tsUnknown';
    const td1 = document.createElement('td'); td1.className = 'ts'; td1.textContent = fmtTime(ts);
    const td2 = document.createElement('td');
    const tag = document.createElement('span');
    tag.className = 'tag ' + (EV_CLASS[type] || 'sys');
    tag.textContent = EV_NAMES[type] || ('type ' + type);
    td2.appendChild(tag);
    const td3 = document.createElement('td'); td3.textContent = evDetail(type, val);
    tr.append(td1, td2, td3);
    b.appendChild(tr);
  }
}

$('evFilter').onchange = renderEvents;
$('evClearFilter').onclick = () => { $('evFilter').value = ''; renderEvents(); };
$('evSort').onclick = () => {
  evNewestFirst = !evNewestFirst;
  $('evSort').innerHTML = evNewestFirst ? 'Newest first &darr;' : 'Oldest first &uarr;';
  renderEvents();
};

/* ── Settings ──────────────────────────────────────────────────────── */
$('unlockBtn').onclick = async () => {
  if (!await ensureAuth()) return;
  await loadSettings();
};

async function loadSettings() {
  let s;
  try { s = await apiJson('/api/settings'); }
  catch (e) { token = ''; sessionStorage.removeItem(TOKEN_KEY); return; }

  $('settingsLocked').hidden = true;
  $('settingsBody').hidden = false;

  document.querySelectorAll('[data-k]').forEach((inp) => {
    const k = inp.dataset.k;
    if (!(k in s)) return;
    if (inp.type === 'checkbox') inp.checked = !!s[k];
    else inp.value = s[k];
  });

  tgList = tgParse(s.tg_chats);
  tgRender();
  loadTrusted();

  /* Secrets come back blank; tell the user whether one is stored. */
  const setMark = (id, on) => { $(id).textContent = on ? '(a value is stored)' : '(not set)'; };
  setMark('wifiPassSet', s.wifi_pass_set);
  setMark('influxTokenSet', s.influx_token_set);
  setMark('tgTokenSet', s.tg_token_set);
  setMark('mqttPassSet', s.mqtt_pass_set);

  scanWifi();
}

function collectSettings(keys) {
  const out = {};
  document.querySelectorAll('[data-k]').forEach((inp) => {
    const k = inp.dataset.k;
    if (keys && !keys.includes(k)) return;
    if (inp.type === 'checkbox') {
      out[k] = inp.checked;
    } else if (inp.type === 'number') {
      /* Numeric settings are typed as JSON numbers on the device; a string
       * would be rejected by the field's range check and silently ignored. */
      const n = parseInt(inp.value, 10);
      if (!isNaN(n)) out[k] = n;
    } else {
      out[k] = inp.value;
    }
  });
  return out;
}

function setStatus(id, msg, cls) {
  const el = $(id);
  el.textContent = msg;
  el.className = 'status ' + (cls || '');
}

$('saveBtn').onclick = async () => {
  setStatus('saveStatus', 'Saving…', 'busy');
  try {
    const r = await api('/api/settings', { method: 'POST', json: collectSettings() });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const j = await r.json();
    setStatus('saveStatus', j.changed + ' setting(s) saved.', 'ok');
    await loadSettings();
    await refreshStatus();
  } catch (e) {
    setStatus('saveStatus',
      e.message === 'unauthorized'
        ? 'Not saved — sign in and try again.'
        : 'Failed: ' + e.message, 'bad');
  }
};

$('wifiSaveBtn').onclick = async () => {
  if (!$('s_wifi_ssid').value.trim()) {
    setStatus('wifiStatus', 'Enter a network name first.', 'bad'); return;
  }
  if (!await askConfirm('Switch Wi-Fi network?',
    'The device will drop off this network and try to join "' + $('s_wifi_ssid').value +
    '". If it cannot, it will bring its own setup Wi-Fi back up so you can retry.')) return;

  setStatus('wifiStatus', 'Saving and reconnecting…', 'busy');
  try {
    const r = await api('/api/wifi', { method: 'POST', json: collectSettings(['wifi_ssid', 'wifi_pass']) });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    setStatus('wifiStatus',
      'Reconnecting. If the address changes, find the device at ' +
      $('s_device_name').value + '.local — otherwise rejoin the setup Wi-Fi.', 'ok');
  } catch (e) {
    setStatus('wifiStatus', 'Failed: ' + e.message, 'bad');
  }
};

async function scanWifi() {
  const sel = $('ssidSel');
  sel.innerHTML = '<option value="">Scanning…</option>';
  try {
    const j = await apiJson('/api/scan');
    sel.innerHTML = '<option value="">— pick a network —</option>';
    for (const ap of j.aps) {
      const o = document.createElement('option');
      o.value = ap.ssid;
      o.textContent = ap.ssid + '  (' + ap.rssi + ' dBm' + (ap.lock ? ', secured' : ', open') + ')';
      sel.appendChild(o);
    }
    if (!j.aps.length) sel.innerHTML = '<option value="">no networks found</option>';
  } catch (e) {
    sel.innerHTML = '<option value="">scan unavailable</option>';
  }
}
$('rescanBtn').onclick = scanWifi;
$('ssidSel').onchange = () => { if ($('ssidSel').value) $('s_wifi_ssid').value = $('ssidSel').value; };

/* ── Help ──────────────────────────────────────────────────────────────
 * The topic list is fetched once and cached; pages are pulled on demand so
 * the whole manual never crosses the wire at once. */
let helpTopics = null;

async function loadHelp() {
  if (helpTopics) return;
  const nav = $('helpNav');
  try {
    const j = await (await fetch('/api/help?v=' + ASSET_V)).json();
    helpTopics = j.topics || [];
    nav.textContent = '';

    let lastCat = null;
    helpTopics.forEach((t) => {
      if (t.c !== lastCat) {
        lastCat = t.c;
        const h = document.createElement('div');
        h.className = 'helpCat';
        h.textContent = t.c;
        nav.appendChild(h);
      }
      const b = document.createElement('button');
      b.className = 'helpLink';
      b.textContent = t.t;
      b.onclick = () => openHelp(t.i, b);
      nav.appendChild(b);
    });

    if (!helpTopics.length) nav.innerHTML = '<div class="helpLoading">No help content.</div>';
  } catch (e) {
    nav.innerHTML = '<div class="helpLoading">Could not load help.</div>';
  }
}

/* Jumps from a settings section straight to the page that documents it.
   Matching on the topic title keeps this readable in the markup and avoids
   hard-coding indices that shift whenever the docs are re-generated. */
async function openHelpByTitle(title) {
  document.querySelectorAll('.navItem')[3].click();
  await loadHelp();
  const t = (helpTopics || []).find((x) => x.t === title);
  if (!t) return;
  const btn = [...document.querySelectorAll('.helpLink')].find((b) => b.textContent === title);
  openHelp(t.i, btn);
}

document.querySelectorAll('.helpFor').forEach((el) => {
  el.onclick = (e) => {
    e.preventDefault();
    openHelpByTitle(el.dataset.help);
  };
});

async function openHelp(id, btn) {
  document.querySelectorAll('.helpLink').forEach((x) => x.classList.toggle('on', x === btn));
  const body = $('helpBody');
  body.innerHTML = '<p class="note">Loading…</p>';
  try {
    const html = await (await fetch('/api/help/page?i=' + id + '&v=' + ASSET_V)).text();
    const t = helpTopics.find((x) => x.i === id);
    body.innerHTML = (t ? '<h2>' + t.t + '</h2>' : '') + html;
    body.scrollIntoView({ block: 'start', behavior: 'smooth' });
  } catch (e) {
    body.innerHTML = '<p class="note">Could not load that page.</p>';
  }
}

/* ── Trusted devices ────────────────────────────────────────────────────
 * Managed through its own endpoint rather than the settings form: an entry
 * is only meaningful if it came from a real authenticated request or a
 * deliberate add, so it is never a free-text field the form can overwrite. */
async function loadTrusted() {
  try {
    const j = await apiJson('/api/trusted');
    const box = $('trustedList');
    box.textContent = '';
    j.devices.forEach((d) => {
      const row = document.createElement('div');
      row.className = 'trustRow' + (d.self ? ' self' : '');
      const info = document.createElement('span');
      info.className = 'grow';
      info.innerHTML = '<span class="ip">' + d.ip + '</span>' +
        (d.self ? '<span class="selfTag">THIS DEVICE</span>' : '') +
        (d.label ? '<br/><span class="meta">' + d.label + '</span>' : '');
      const del = document.createElement('button');
      del.textContent = 'Remove';
      del.onclick = () => trustEdit('remove&ip=' + encodeURIComponent(d.ip),
                                    d.self ? 'You will be asked for the password again.'
                                           : 'Removed ' + d.ip + '.');
      row.append(info, del);
      box.appendChild(row);
    });
    setStatus('trustStatus', j.n + ' of ' + j.max + ' slots used. You are ' + (j.you || 'unknown') + '.', '');
  } catch (e) {
    setStatus('trustStatus', 'Could not load the list: ' + e.message, 'bad');
  }
}

async function trustEdit(qs, okMsg) {
  try {
    const r = await api('/api/trusted?action=' + qs, { method: 'POST' });
    if (!r.ok) {
      const j = await r.json().catch(() => ({}));
      setStatus('trustStatus', j.error || ('HTTP ' + r.status), 'bad');
      return;
    }
    setStatus('trustStatus', okMsg, 'ok');
    await loadTrusted();
    await refreshStatus();
  } catch (e) {
    setStatus('trustStatus', 'Failed: ' + e.message, 'bad');
  }
}

$('trustAddBtn').onclick = () => {
  const ip = $('trustNewIp').value.trim();
  const label = $('trustNewLabel').value.trim();
  if (!ip) { setStatus('trustStatus', 'Enter an address first.', 'bad'); return; }
  $('trustNewIp').value = $('trustNewLabel').value = '';
  trustEdit('add&ip=' + encodeURIComponent(ip) + '&label=' + encodeURIComponent(label),
            'Added ' + ip + '.');
};

$('trustClearBtn').onclick = async () => {
  if (!await askConfirm('Remove all trusted devices?',
    'Every device, including this one, will be asked for the password next time.', true)) return;
  await trustEdit('clear', 'List emptied.');
};

/* ── Telegram recipients ────────────────────────────────────────────────
 * The list is held in one packed "id:nick|id:nick" string so it rides the
 * ordinary settings save. These helpers keep the visible list and that
 * hidden field in step. */
let tgList = [];

function tgParse(packed) {
  return (packed || '').split('|').filter(Boolean).map((rec) => {
    const i = rec.indexOf(':');
    return i < 0 ? { id: rec, nick: '' }
                 : { id: rec.slice(0, i), nick: rec.slice(i + 1) };
  });
}

function tgPack() {
  return tgList.map((c) => c.id + (c.nick ? ':' + c.nick : '')).join('|');
}

/* `:` and `|` are the record separators, so they cannot survive in a value. */
const tgClean = (v) => (v || '').replace(/[:|]/g, '').trim();

function tgRender() {
  const box = $('tgRecipients');
  box.textContent = '';
  tgList.forEach((c, idx) => {
    const row = document.createElement('div');
    row.className = 'tgRow';
    const info = document.createElement('span');
    info.className = 'grow';
    info.innerHTML = '<span class="nick">' + (c.nick || 'unnamed') + '</span><br/>' +
                     '<span class="cid">' + c.id + '</span>';
    const del = document.createElement('button');
    del.textContent = 'Remove';
    del.onclick = () => { tgList.splice(idx, 1); tgRender(); };
    row.append(info, del);
    box.appendChild(row);
  });
  $('s_tg_chats').value = tgPack();
}

$('tgAddBtn').onclick = () => {
  const id = tgClean($('tgNewId').value);
  const nick = tgClean($('tgNewNick').value);
  if (!/^-?\d{4,}$/.test(id)) {
    setStatus('tgChatList', 'A chat ID is a number — use "Find chat IDs" if you are unsure.', 'bad');
    return;
  }
  if (tgList.some((c) => c.id === id)) {
    setStatus('tgChatList', 'That recipient is already on the list.', 'bad');
    return;
  }
  if (tgList.length >= 8) {
    setStatus('tgChatList', 'Eight recipients is the maximum.', 'bad');
    return;
  }
  tgList.push({ id, nick: nick || ('Chat ' + id) });
  $('tgNewId').value = $('tgNewNick').value = '';
  setStatus('tgChatList', 'Added — press Save all settings to keep it.', 'ok');
  tgRender();
};

/* Reads the chat ID back from a message the user actually sent, which is the
   only way to get it that cannot be confused with the bot's own id. */
$('tgFindBtn').onclick = async () => {
  setStatus('tgChatList', 'Asking Telegram… (send your bot a message first if you have not)', 'busy');
  try {
    const j = await apiJson('/api/telegram-chats', { method: 'POST' });
    if (j.error) { setStatus('tgChatList', j.error, 'bad'); return; }
    if (!j.n)    { setStatus('tgChatList', j.hint || 'No recent chats found.', 'bad'); return; }

    const box = $('tgChatList');
    box.className = 'status';
    box.textContent = 'Tap the one that is you: ';
    j.chats.forEach((c) => {
      const b = document.createElement('button');
      b.className = 'ghost';
      b.style.cssText = 'margin:4px 4px 0 0;padding:5px 10px;font-size:12px';
      b.textContent = (c.name || c.id) + ' (' + c.id + ')' + (c.type === 'private' ? '' : ' · ' + c.type);
      b.onclick = () => {
        if (tgList.some((x) => x.id === String(c.id))) {
          setStatus('tgChatList', 'Already on the list.', 'busy');
          return;
        }
        tgList.push({ id: String(c.id), nick: tgClean(c.name) || ('Chat ' + c.id) });
        tgRender();
        setStatus('tgChatList', 'Added ' + (c.name || c.id) + ' — press Save all settings.', 'ok');
      };
      box.appendChild(b);
    });
  } catch (e) {
    setStatus('tgChatList', 'Failed: ' + e.message, 'bad');
  }
};

$('testNotifyBtn').onclick = async () => {
  setStatus('notifyStatus', 'Sending… (save your settings first if you just changed them)', 'busy');
  try {
    const j = await apiJson('/api/notify-test', { method: 'POST' });
    setStatus('notifyStatus', j.detail, j.ok ? 'ok' : 'bad');
  } catch (e) {
    setStatus('notifyStatus', 'Failed: ' + e.message, 'bad');
  }
};

$('alarmReportBtn').onclick = async () => {
  setStatus('alarmReportStatus', 'Sending…', 'busy');
  try {
    const j = await apiJson('/api/alarm-report', { method: 'POST' });
    const what = 'Immediate=' + j.immediate + ', LeftOpen=' + j.leftOpen;
    setStatus('alarmReportStatus', j.detail + ' — ' + what, j.ok ? 'ok' : 'bad');
  } catch (e) {
    setStatus('alarmReportStatus', 'Failed: ' + e.message, 'bad');
  }
};

$('pwBtn').onclick = async () => {
  const cur = $('pwCur').value, nw = $('pwNew').value;
  if (nw.length < 4 || nw.length > 32) {
    setStatus('pwStatus', 'New password must be 4–32 characters.', 'bad'); return;
  }
  setStatus('pwStatus', 'Changing…', 'busy');
  try {
    const r = await api('/api/password', { method: 'POST', json: { current: cur, next: nw } });
    if (r.status === 403) { setStatus('pwStatus', 'Current password is wrong.', 'bad'); return; }
    if (!r.ok) throw new Error('HTTP ' + r.status);
    $('pwCur').value = $('pwNew').value = '';
    token = ''; sessionStorage.removeItem(TOKEN_KEY);
    setStatus('pwStatus', 'Password changed. You will be asked for the new one next time.', 'ok');
  } catch (e) {
    setStatus('pwStatus', 'Failed: ' + e.message, 'bad');
  }
};

$('rebootBtn').onclick = async () => {
  if (!await askConfirm('Reboot the device?', 'The door will not move. The UI reconnects on its own.')) return;
  setStatus('maintStatus', 'Rebooting…', 'busy');
  try { await api('/api/reboot', { method: 'POST' }); } catch (e) { /* connection drops, expected */ }
};

$('clearHistBtn').onclick = async () => {
  if (!await askConfirm('Erase on-device history?',
    'Every stored event and sample is deleted. This cannot be undone.', true)) return;
  try {
    await api('/api/history/clear', { method: 'POST' });
    setStatus('maintStatus', 'History cleared.', 'ok');
    loadHistory();
  } catch (e) { setStatus('maintStatus', 'Failed: ' + e.message, 'bad'); }
};

$('factoryBtn').onclick = async () => {
  if (!await askConfirm('Factory reset?',
    'Wi-Fi credentials, tokens, the admin password and all settings are erased. ' +
    'The device reboots into first-time setup.', true)) return;
  const pw = await askPassword('Confirm with your password', 'This is irreversible.');
  if (!pw) return;
  try {
    const r = await api('/api/factory-reset', { method: 'POST', json: { password: pw } });
    if (r.status === 403) { setStatus('maintStatus', 'Wrong password.', 'bad'); return; }
    setStatus('maintStatus', 'Reset. Rebooting into setup mode…', 'ok');
  } catch (e) { setStatus('maintStatus', 'Failed: ' + e.message, 'bad'); }
};

/* ── Calibration ───────────────────────────────────────────────────────
 * Driven from the same /api/status poll as everything else, so the live
 * readout and the position needle stay in step with the rest of the page. */
function updateCalPanel(j) {
  const mv = $('calLiveMv');
  if (!mv) return;

  mv.textContent = j.doorMv;
  mv.style.color = j.doorClosed ? 'var(--ok)' : 'var(--bad)';
  $('calLiveRaw').textContent = 'raw ' + j.doorRaw + (j.doorCalibrated ? '' : ' · uncalibrated ADC');

  const c = j.doorClosedMv, o = j.doorOpenMv;
  $('calClosedVal').textContent = c ? ('captured: ' + c + ' mV') : 'not captured yet';
  $('calOpenVal').textContent   = o ? ('captured: ' + o + ' mV') : 'not captured yet';
  $('calStep1').classList.toggle('done', !!c);
  $('calStep2').classList.toggle('done', !!o);

  /* Scale the strip to the two captured points with a margin, so the needle
     has somewhere to go when the reading drifts outside them. */
  const bar = $('calBar');
  if (!c || !o) {
    bar.style.opacity = '0.35';
    ['calZoneClosed','calZoneOpen','calMarkClosed','calMarkOpen','calMarkTrip','calTagClosed','calTagOpen']
      .forEach((id) => { $(id).style.display = 'none'; });
    $('calNeedle').style.left = Math.max(0, Math.min(100, (j.doorMv / 3300) * 100)) + '%';
    return;
  }
  bar.style.opacity = '1';
  ['calZoneClosed','calZoneOpen','calMarkClosed','calMarkOpen','calMarkTrip','calTagClosed','calTagOpen']
    .forEach((id) => { $(id).style.display = ''; });

  const lo = Math.min(c, o), hi = Math.max(c, o);
  const pad = Math.max(60, (hi - lo) * 0.25);
  const from = lo - pad, span = (hi + pad) - from;
  const pct = (v) => Math.max(0, Math.min(100, ((v - from) / span) * 100));

  const closedPct = pct(c), openPct = pct(o), tripPct = pct(j.doorThresholdMv);
  $('calMarkClosed').style.left = closedPct + '%';
  $('calMarkOpen').style.left   = openPct + '%';
  $('calMarkTrip').style.left   = tripPct + '%';
  $('calNeedle').style.left     = pct(j.doorMv) + '%';

  /* Colour the half of the strip that means "closed", whichever side it is. */
  if (c > o) {
    $('calZoneClosed').style.cssText = 'left:' + tripPct + '%;right:0';
    $('calZoneOpen').style.cssText   = 'left:0;right:' + (100 - tripPct) + '%';
  } else {
    $('calZoneClosed').style.cssText = 'left:0;right:' + (100 - tripPct) + '%';
    $('calZoneOpen').style.cssText   = 'left:' + tripPct + '%;right:0';
  }
  $('calZoneClosed').className = 'zone closed';
  $('calZoneOpen').className   = 'zone open';
  $('calTagClosed').style.left = 'calc(' + closedPct + '% + 4px)';
  $('calTagOpen').style.left   = 'calc(' + openPct + '% + 4px)';

  const gap = hi - lo;
  const v = $('calVerdict');
  if (j.doorCalValid) {
    v.className = gap >= 500 ? 'good' : 'warn';
    v.textContent = (gap >= 500 ? '✓ Calibrated. ' : '⚠ Calibrated, but only just. ') +
      'Closed ' + c + ' mV, open ' + o + ' mV — ' + gap + ' mV apart. ' +
      'Trip point ' + j.doorThresholdMv + ' mV' +
      (gap < 500 ? '. A larger separation would be more reliable; check the sensor gap and wiring.' : '.');
  } else {
    v.className = 'bad';
    v.textContent = '✗ Not calibrated: the two readings are only ' + gap +
      ' mV apart, which is not enough to tell the positions apart. ' +
      'If the value barely moves when the door does, the sensor is probably not ' +
      'reaching the input — check the wiring and whether it needs a pull-up.';
  }
}

async function capture(point, btn, label) {
  if (!await ensureAuth()) return;
  const original = btn.textContent;
  btn.disabled = true;
  btn.textContent = 'Measuring…';
  try {
    const r = await api('/api/calibrate?point=' + point, { method: 'POST' });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const j = await r.json();
    if (point !== 'clear') {
      const noisy = j.spread > 80;
      await askAlert(label + ' captured',
        j.measured + ' mV' + (noisy
          ? ' — but it wandered ' + j.spread + ' mV while measuring, which is a lot. ' +
            'If it does that with the door still, the input may be floating.'
          : ' (steady to within ' + j.spread + ' mV).'));
    }
    await refreshStatus();
  } catch (e) {
    await askAlert('Calibration failed', String(e.message));
  } finally {
    btn.disabled = false;
    btn.textContent = original;
  }
}

$('calClosedBtn').onclick = () => capture('closed', $('calClosedBtn'), 'Closed position');
$('calOpenBtn').onclick   = () => capture('open',   $('calOpenBtn'),   'Open position');
$('calClearBtn').onclick  = async () => {
  if (!await askConfirm('Clear calibration?',
    'The device will fall back to the manual trip point until you calibrate again.')) return;
  await capture('clear', $('calClearBtn'), '');
};

/* ── OTA ───────────────────────────────────────────────────────────── */
let otaFile = null;
const drop = $('otaDrop');

function pickFile(f) {
  otaFile = f;
  $('otaLabel').textContent = '✓ ' + f.name + '  (' + (f.size / 1024).toFixed(0) + ' KB)';
  drop.classList.add('has');
  $('otaBtn').disabled = false;
  setStatus('otaStatus', 'Ready to flash.', '');
}
drop.onclick = () => $('otaFile').click();
drop.ondragover = (e) => { e.preventDefault(); drop.classList.add('over'); };
drop.ondragleave = () => drop.classList.remove('over');
drop.ondrop = (e) => {
  e.preventDefault(); drop.classList.remove('over');
  if (e.dataTransfer.files[0]) pickFile(e.dataTransfer.files[0]);
};
$('otaFile').onchange = (e) => { if (e.target.files[0]) pickFile(e.target.files[0]); };

$('otaBtn').onclick = async () => {
  if (!otaFile) return;
  if (!await ensureAuth()) return;
  if (!await askConfirm('Flash new firmware?', otaFile.name + ' will replace the running firmware.')) return;

  $('otaBtn').disabled = true;
  $('otaBar').style.display = 'block';
  $('otaFill').className = '';
  $('otaFill').style.width = '0';

  const xhr = new XMLHttpRequest();
  xhr.upload.onprogress = (e) => {
    if (!e.lengthComputable) return;
    const p = Math.round((e.loaded / e.total) * 100);
    $('otaFill').style.width = p + '%';
    setStatus('otaStatus', 'Uploading… ' + p + '%', 'busy');
  };
  xhr.onload = () => {
    if (xhr.status !== 200) {
      setStatus('otaStatus', 'Error (HTTP ' + xhr.status + '): ' + (xhr.responseText || 'unknown'), 'bad');
      $('otaBtn').disabled = false;
      return;
    }
    $('otaFill').style.width = '100%';
    setStatus('otaStatus', 'Flashed. Rebooting…', 'busy');
    let n = 0;
    const t = setInterval(() => {
      fetch('/api/status').then((r) => {
        if (!r.ok) return;
        clearInterval(t);
        $('otaFill').className = 'done';
        setStatus('otaStatus', 'Update complete — device is back online.', 'ok');
      }).catch(() => setStatus('otaStatus', 'Rebooting… (' + (++n * 2) + 's)', 'busy'));
    }, 2000);
  };
  xhr.onerror = () => {
    setStatus('otaStatus', 'Network error during upload.', 'bad');
    $('otaBtn').disabled = false;
  };
  xhr.open('POST', '/api/update');
  xhr.setRequestHeader('Content-Type', 'application/octet-stream');
  xhr.setRequestHeader('X-Auth-Token', token);
  xhr.send(otaFile);
};

/* ── Pollers ───────────────────────────────────────────────────────────
 * Staggered so a page load does not open four sockets at once — the
 * server's pool is small and a simultaneous burst causes accept errors. */
refreshStatus();
setInterval(refreshStatus, 1500);
setTimeout(() => { refreshNetwork(); setInterval(refreshNetwork, 8000); }, 300);
setTimeout(() => { refreshLogs();    setInterval(refreshLogs, 1500); }, 600);
)JS";

/* ═══════════════════════════════════════════════════════════════════════
 * First-time setup wizard — GET / while unprovisioned
 *
 * Deliberately self-contained: a device that has never been configured
 * should not depend on a second HTTP request succeeding, and the user is
 * usually on a phone joined to our SoftAP with no internet at all.
 * ═══════════════════════════════════════════════════════════════════════ */

extern const char k_setup_html[] = R"SETUP(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"/>
<meta name="color-scheme" content="dark light"/>
<title>Set up Garage Gadget</title>
<style>
:root{--bg:#0d1117;--panel:#161b22;--panel2:#0b0e13;--line:#272e38;--fg:#e6edf3;
  --muted:#8b949e;--accent:#2f81f7;--accent2:#1f6feb;--ok:#3fb950;--bad:#f85149}
@media (prefers-color-scheme:light){:root{--bg:#f4f6f9;--panel:#fff;--panel2:#eef1f5;
  --line:#d5dbe3;--fg:#111820;--muted:#5b6673}}
*{box-sizing:border-box}
body{margin:0;padding:18px;background:var(--bg);color:var(--fg);max-width:560px;margin-inline:auto;
  font:15px/1.55 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
h1{font-size:21px;margin:0 0 4px}
h2{font-size:17px;margin:0 0 10px}
p{color:var(--muted);font-size:13.5px;margin:0 0 14px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:18px;margin-bottom:14px}
.dots{display:flex;gap:6px;justify-content:center;margin:0 0 16px}
.dots i{width:26px;height:4px;border-radius:2px;background:var(--line)}
.dots i.on{background:var(--accent)}
label{display:block;font-size:12px;color:var(--muted);margin:0 0 4px}
input[type=text],input[type=password],select{width:100%;background:var(--panel2);
  border:1px solid var(--line);border-radius:8px;color:var(--fg);font:inherit;padding:10px;outline:none;margin-bottom:12px}
input:focus,select:focus{border-color:var(--accent)}
input.bad{border-color:var(--bad)}
button{font:inherit;font-weight:600;border:none;border-radius:8px;padding:11px 16px;
  background:var(--accent2);color:#fff;cursor:pointer}
button.ghost{background:transparent;border:1px solid var(--line);color:var(--fg)}
button:disabled{background:#3a3f45;color:#8b949e;cursor:not-allowed}
.nav{display:flex;gap:8px;margin-top:16px}
.nav button{flex:1}
.step{display:none}.step.on{display:block}
.tog{display:flex;align-items:center;justify-content:space-between;gap:12px;
  padding:10px 0;border-bottom:1px solid var(--line)}
.tog:last-of-type{border-bottom:none}
.tog span{font-size:13.5px}
.tog small{display:block;color:var(--muted);font-size:11.5px}
.sw{position:relative;width:44px;height:24px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sl{position:absolute;inset:0;background:#3a3f45;border-radius:24px;transition:.2s;cursor:pointer}
.sl:before{content:'';position:absolute;height:18px;width:18px;left:3px;bottom:3px;background:#fff;
  border-radius:50%;transition:.2s}
input:checked + .sl{background:var(--accent2)}
input:checked + .sl:before{transform:translateX(20px)}
.hid{display:none}
.msg{font-size:12.5px;min-height:18px;margin-top:8px}
.msg.bad{color:var(--bad)}.msg.ok{color:var(--ok)}
.done{text-align:center;padding:16px 0}
.done .big{font-size:40px;margin-bottom:8px}
code{background:var(--panel2);padding:1px 5px;border-radius:4px;font-size:12.5px}
</style></head><body>

<h1>Garage Gadget</h1>
<p>First-time setup. Everything except Wi-Fi and the admin password is optional &mdash; you can change any of it later.</p>
<div class="dots"><i class="on"></i><i></i><i></i><i></i><i></i></div>

<!-- 1 ─ Wi-Fi -->
<div class="step on" id="s0"><div class="card">
  <h2>1. Connect to your Wi-Fi</h2>
  <p>Pick your home network so the device can reach the clock, and your dashboards and notifications if you set them up.</p>
  <label for="pick">Nearby networks</label>
  <select id="pick"><option value="">Scanning&hellip;</option></select>
  <label for="ssid">Network name (SSID)</label>
  <input type="text" id="ssid" autocomplete="off"/>
  <label for="pass">Password</label>
  <input type="password" id="pass" autocomplete="new-password"/>
  <button class="ghost" id="rescan" style="padding:7px 12px;font-size:12.5px">Rescan</button>
  <div class="msg" id="m0"></div>
  <div class="nav"><button id="n0">Continue</button></div>
</div></div>

<!-- 2 ─ Device -->
<div class="step" id="s1"><div class="card">
  <h2>2. Name this device</h2>
  <p>Used for the local address and to tag your data. Letters, digits and hyphens.</p>
  <label for="dname">Device name</label>
  <input type="text" id="dname" value="garage"/>
  <p>You will reach the dashboard at <code id="hostPreview">http://garage.local</code> once setup finishes.</p>
  <label for="tz">Timezone (POSIX TZ string)</label>
  <input type="text" id="tz" value="EST5EDT,M3.2.0/2,M11.1.0/2"/>
  <div class="msg" id="m1"></div>
  <div class="nav"><button class="ghost" data-b="0">Back</button><button id="n1">Continue</button></div>
</div></div>

<!-- 3 ─ Data -->
<div class="step" id="s2"><div class="card">
  <h2>3. Where should data go?</h2>
  <p>By default everything stays on the device: the dashboard charts the last 10 days from its own storage. Add InfluxDB only if you already run one.</p>
  <div class="tog"><span>Send telemetry to InfluxDB<small>Leave off for on-device storage only</small></span>
    <label class="sw"><input type="checkbox" id="influxOn"><span class="sl"></span></label></div>
  <div id="influxFields" class="hid" style="margin-top:14px">
    <label for="ifxUrl">URL</label><input type="text" id="ifxUrl" placeholder="https://eu-central-1-1.aws.cloud2.influxdata.com"/>
    <label for="ifxTok">API token</label><input type="password" id="ifxTok"/>
    <label for="ifxOrg">Organisation</label><input type="text" id="ifxOrg"/>
    <label for="ifxBucket">Bucket</label><input type="text" id="ifxBucket"/>
  </div>
  <label for="grafana" style="margin-top:14px">Grafana dashboard URL (optional)</label>
  <input type="text" id="grafana" placeholder="https://yourorg.grafana.net/d/abc123"/>
  <p style="margin:0">Adds an &ldquo;Open in Grafana&rdquo; link to the dashboard. The device never connects to Grafana itself.</p>
  <div class="msg" id="m2"></div>
  <div class="nav"><button class="ghost" data-b="1">Back</button><button id="n2">Continue</button></div>
</div></div>

<!-- 4 ─ Notifications -->
<div class="step" id="s3"><div class="card">
  <h2>4. Alerts on your phone</h2>
  <p>The device posts straight to these services when an alarm trips. No server in between, nothing to host.</p>

  <div class="tog"><span>Telegram<small>1. &commat;BotFather &rarr; <code>/newbot</code> &rarr; copy the whole token
    (<code>123456789:AAH...</code>). 2. &commat;userinfobot for your chat ID.
    <b>3. Send your new bot a message first</b> &mdash; Telegram will not let it message you otherwise.</small></span>
    <label class="sw"><input type="checkbox" id="tgOn"><span class="sl"></span></label></div>
  <div id="tgFields" class="hid" style="margin-top:12px">
    <label for="tgTok">Bot token</label><input type="password" id="tgTok" placeholder="123456:AA..."/>
    <label for="tgChat">Chat ID</label><input type="text" id="tgChat"/>
  </div>

  <div class="tog"><span>ntfy<small>No account. Choose a long topic nobody could guess, then subscribe to it in the ntfy app.</small></span>
    <label class="sw"><input type="checkbox" id="ntfyOn"><span class="sl"></span></label></div>
  <div id="ntfyFields" class="hid" style="margin-top:12px">
    <label for="ntfySrv">Server</label><input type="text" id="ntfySrv" value="https://ntfy.sh"/>
    <label for="ntfyTopic">Topic</label><input type="text" id="ntfyTopic"/>
  </div>

  <div class="msg" id="m3"></div>
  <div class="nav"><button class="ghost" data-b="2">Back</button><button id="n3">Continue</button></div>
</div></div>

<!-- 5 ─ Password -->
<div class="step" id="s4"><div class="card">
  <h2>5. Set an admin password</h2>
  <p>Required. It protects the door button and every setting on this device. Choose something only you know &mdash; anyone on your network who has it can open your garage.</p>
  <label for="pw1">New password (4&ndash;32 characters)</label>
  <input type="password" id="pw1" autocomplete="new-password"/>
  <label for="pw2">Repeat password</label>
  <input type="password" id="pw2" autocomplete="new-password"/>
  <div class="msg" id="m4"></div>
  <div class="nav"><button class="ghost" data-b="3">Back</button><button id="finish">Finish setup</button></div>
</div></div>

<!-- 6 ─ Done -->
<div class="step" id="s5"><div class="card done">
  <div class="big" id="doneIcon">&#8987;</div>
  <h2 id="doneTitle">Joining your Wi-Fi&hellip;</h2>
  <p id="doneMsg">Stay on this setup network for a moment &mdash; the device will report
  its new address here as soon as it connects.</p>
  <div id="doneOk" style="display:none">
    <p style="color:var(--ok);font-weight:600;margin-bottom:6px">Connected.</p>
    <p style="margin-bottom:6px">Your device is now at:</p>
    <p style="font-size:19px;font-weight:700;margin-bottom:14px"><code id="doneIp">&mdash;</code></p>
    <p>Reconnect your phone to your normal Wi-Fi, then open that address
    (or <code id="doneHost">http://garage.local</code>).</p>
  </div>
  <div id="doneWait">
    <p style="margin:0">If this does not connect within a minute, the Wi-Fi details were
    probably wrong. The device keeps the <b id="doneAp">setup network</b> running so you
    can come back here and correct them.</p>
  </div>
</div></div>

<script>
'use strict';
const $ = (i) => document.getElementById(i);
let step = 0;

function show(n) {
  step = n;
  document.querySelectorAll('.step').forEach((s, i) => s.classList.toggle('on', i === n));
  document.querySelectorAll('.dots i').forEach((d, i) => d.classList.toggle('on', i <= n));
  window.scrollTo(0, 0);
}
document.querySelectorAll('[data-b]').forEach((b) => { b.onclick = () => show(+b.dataset.b); });

function msg(id, text, cls) { const e = $(id); e.textContent = text; e.className = 'msg ' + (cls || ''); }

/* ── Wi-Fi scan ── */
async function scan() {
  const sel = $('pick');
  sel.innerHTML = '<option value="">Scanning…</option>';
  try {
    const j = await (await fetch('/api/scan')).json();
    sel.innerHTML = '<option value="">— pick a network —</option>';
    for (const ap of j.aps) {
      const o = document.createElement('option');
      o.value = ap.ssid;
      o.textContent = ap.ssid + '  (' + ap.rssi + ' dBm' + (ap.lock ? ', secured' : ', open') + ')';
      sel.appendChild(o);
    }
    if (!j.aps.length) sel.innerHTML = '<option value="">no networks found — type it below</option>';
  } catch (e) {
    sel.innerHTML = '<option value="">scan unavailable — type it below</option>';
  }
}
$('pick').onchange = () => { if ($('pick').value) $('ssid').value = $('pick').value; };
$('rescan').onclick = scan;
scan();

$('n0').onclick = () => {
  if (!$('ssid').value.trim()) { msg('m0', 'Enter your network name.', 'bad'); $('ssid').classList.add('bad'); return; }
  $('ssid').classList.remove('bad'); msg('m0', '');
  show(1);
};

/* ── Device name ── */
$('dname').oninput = () => {
  const v = $('dname').value.trim() || 'garage';
  $('hostPreview').textContent = 'http://' + v + '.local';
  $('doneHost').textContent = 'http://' + v + '.local';
};
$('n1').onclick = () => {
  const v = $('dname').value.trim();
  if (!/^[A-Za-z0-9][A-Za-z0-9-]{0,30}$/.test(v)) {
    msg('m1', 'Use letters, digits and hyphens, starting with a letter or digit.', 'bad'); return;
  }
  msg('m1', ''); show(2);
};

/* ── Conditional field groups ── */
const bind = (cb, group) => {
  $(cb).onchange = () => $(group).classList.toggle('hid', !$(cb).checked);
};
bind('influxOn', 'influxFields');
bind('tgOn', 'tgFields');
bind('ntfyOn', 'ntfyFields');

$('n2').onclick = () => {
  if ($('influxOn').checked) {
    for (const id of ['ifxUrl', 'ifxTok', 'ifxOrg', 'ifxBucket']) {
      if (!$(id).value.trim()) {
        msg('m2', 'Fill in every InfluxDB field, or switch InfluxDB off to skip it.', 'bad');
        $(id).classList.add('bad'); return;
      }
      $(id).classList.remove('bad');
    }
  }
  msg('m2', ''); show(3);
};

$('n3').onclick = () => {
  if ($('tgOn').checked && (!$('tgTok').value.trim() || !$('tgChat').value.trim())) {
    msg('m3', 'Telegram needs both a bot token and a chat ID.', 'bad'); return;
  }
  if ($('ntfyOn').checked && !$('ntfyTopic').value.trim()) {
    msg('m3', 'Choose an ntfy topic, or switch ntfy off.', 'bad'); return;
  }
  msg('m3', ''); show(4);
};

/* ── Finish ── */
$('finish').onclick = async () => {
  const p1 = $('pw1').value, p2 = $('pw2').value;
  if (p1.length < 4 || p1.length > 32) { msg('m4', 'Password must be 4–32 characters.', 'bad'); return; }
  if (p1 !== p2) { msg('m4', 'The two passwords do not match.', 'bad'); return; }

  const payload = {
    wifi_ssid: $('ssid').value.trim(),
    wifi_pass: $('pass').value,
    device_name: $('dname').value.trim(),
    timezone: $('tz').value.trim(),
    influx_on: $('influxOn').checked,
    influx_url: $('ifxUrl').value.trim(),
    influx_token: $('ifxTok').value,
    influx_org: $('ifxOrg').value.trim(),
    influx_bucket: $('ifxBucket').value.trim(),
    grafana_url: $('grafana').value.trim(),
    tg_on: $('tgOn').checked,
    tg_token: $('tgTok').value,
    tg_chat: $('tgChat').value.trim(),
    ntfy_on: $('ntfyOn').checked,
    ntfy_server: $('ntfySrv').value.trim(),
    ntfy_topic: $('ntfyTopic').value.trim(),
    password: p1
  };

  $('finish').disabled = true;
  msg('m4', 'Saving…', '');
  try {
    const r = await fetch('/api/setup/complete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload)
    });
    if (!r.ok) throw new Error('HTTP ' + r.status);
    const j = await r.json();
    if (j.apSsid) $('doneAp').textContent = j.apSsid;
    show(5);
    watchForStationIp();
  } catch (e) {
    msg('m4', 'Could not save: ' + e.message, 'bad');
    $('finish').disabled = false;
  }
};

/* The setup AP deliberately stays up for a few minutes after the station
 * joins, so we can poll from the phone that just finished the wizard and
 * show the address the device actually landed on. Without this the AP would
 * vanish mid-page and the whole thing would look like a failure. */
function watchForStationIp() {
  let tries = 0;
  const t = setInterval(async () => {
    tries++;
    try {
      const n = await (await fetch('/api/network', { cache: 'no-store' })).json();
      if (n.ip) {
        clearInterval(t);
        $('doneIcon').innerHTML = '&#10003;';
        $('doneTitle').textContent = 'Setup complete';
        $('doneMsg').textContent = '';
        $('doneIp').textContent = 'http://' + n.ip + (n.port === 80 ? '' : ':' + n.port);
        $('doneOk').style.display = '';
        $('doneWait').style.display = 'none';
      }
    } catch (e) {
      /* The AP drops as soon as the phone roams back to the home network;
         that is a normal way for this to end, not an error. */
    }
    if (tries > 90) clearInterval(t);
  }, 2000);
}
</script>
</body></html>
)SETUP";

/* ═══════════════════════════════════════════════════════════════════════
 * Recovery page — GET / while a password reset is armed
 * ═══════════════════════════════════════════════════════════════════════ */

extern const char k_recovery_html[] = R"RECOV(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"/>
<meta name="color-scheme" content="dark light"/>
<title>Recover Garage Gadget</title>
<style>
:root{--bg:#0d1117;--panel:#161b22;--panel2:#0b0e13;--line:#272e38;--fg:#e6edf3;
  --muted:#8b949e;--accent:#2f81f7;--accent2:#1f6feb;--ok:#3fb950;--warn:#d29922;--bad:#f85149}
@media (prefers-color-scheme:light){:root{--bg:#f4f6f9;--panel:#fff;--panel2:#eef1f5;
  --line:#d5dbe3;--fg:#111820;--muted:#5b6673}}
*{box-sizing:border-box}
body{margin:0;padding:18px;background:var(--bg);color:var(--fg);max-width:520px;margin-inline:auto;
  font:15px/1.55 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
h1{font-size:21px;margin:0 0 4px}
h2{font-size:16px;margin:0 0 10px}
p{color:var(--muted);font-size:13.5px;margin:0 0 14px}
.card{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:18px;margin-bottom:14px}
.banner{background:rgba(210,153,34,.14);border:1px solid var(--warn);color:var(--warn);
  border-radius:10px;padding:12px 14px;font-size:13px;margin-bottom:16px}
label{display:block;font-size:12px;color:var(--muted);margin:0 0 4px}
input[type=text],input[type=password]{width:100%;background:var(--panel2);border:1px solid var(--line);
  border-radius:8px;color:var(--fg);font:inherit;padding:10px;outline:none;margin-bottom:12px}
input:focus{border-color:var(--accent)}
input.bad{border-color:var(--bad)}
button{font:inherit;font-weight:600;border:none;border-radius:8px;padding:11px 16px;width:100%;
  background:var(--accent2);color:#fff;cursor:pointer}
button:disabled{background:#3a3f45;color:#8b949e;cursor:not-allowed}
.tog{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:6px 0 12px}
.sw{position:relative;width:44px;height:24px;flex:none}
.sw input{opacity:0;width:0;height:0}
.sl{position:absolute;inset:0;background:#3a3f45;border-radius:24px;transition:.2s;cursor:pointer}
.sl:before{content:'';position:absolute;height:18px;width:18px;left:3px;bottom:3px;background:#fff;
  border-radius:50%;transition:.2s}
input:checked + .sl{background:var(--accent2)}
input:checked + .sl:before{transform:translateX(20px)}
.hid{display:none}
.msg{font-size:12.5px;min-height:18px;margin-top:8px}
.msg.bad{color:var(--bad)}.msg.ok{color:var(--ok)}
.keeps{font-size:12.5px;color:var(--muted);margin:0}
.keeps li{margin-bottom:3px}
</style></head><body>

<h1>Recover this device</h1>
<div class="banner"><b>Recovery mode.</b> Someone held the button on the device for
ten seconds. The old admin password no longer works and every trusted device has
been removed. Set a new password below to finish.</div>

<div class="card" id="form">
  <h2>New admin password</h2>
  <label for="p1">New password (4&ndash;32 characters)</label>
  <input type="password" id="p1" autocomplete="new-password"/>
  <label for="p2">Repeat password</label>
  <input type="password" id="p2" autocomplete="new-password"/>

  <div class="tog" id="wifiTog"><span id="wifiLabel">Also change the Wi-Fi network<small style="display:block;color:var(--muted);font-size:11.5px" id="wifiHint">
    Only if the device cannot reach your network any more.</small></span>
    <label class="sw"><input type="checkbox" id="wifiOn"><span class="sl"></span></label></div>
  <div id="wifiFields" class="hid">
    <label for="ssid">Network name (SSID)</label>
    <input type="text" id="ssid" autocomplete="off"/>
    <label for="wpass">Wi-Fi password</label>
    <input type="password" id="wpass" autocomplete="new-password"/>
  </div>

  <button id="go">Set password</button>
  <div class="msg" id="m"></div>
</div>

<div class="card">
  <h2>Nothing else is lost</h2>
  <p class="keeps">Recovery only replaces the password. These are kept exactly as they were:</p>
  <ul class="keeps">
    <li>Wi-Fi credentials, unless you change them above</li>
    <li>Door sensor calibration</li>
    <li>InfluxDB and Grafana settings</li>
    <li>Telegram and ntfy recipients</li>
    <li>The complete event history</li>
  </ul>
</div>

<div class="card hid" id="done">
  <h2 style="color:var(--ok)">&#10003; Password set</h2>
  <p>You can sign in again with the new password. Trusted devices were cleared,
  so each one is asked once more the next time it is used.</p>
  <p id="where"></p>
</div>

<script>
'use strict';
const $ = (i) => document.getElementById(i);
let wifiRequired = false;

$('wifiOn').onchange = () => {
  if (wifiRequired) { $('wifiOn').checked = true; return; }
  $('wifiFields').classList.toggle('hid', !$('wifiOn').checked);
};

/* If the device could not join the stored network, new credentials are not
   optional: the hotspot serving this page vanishes on the next reboot, so
   finishing without a working network would strand the unit. */
async function checkNetwork() {
  try {
    const n = await (await fetch('/api/network', { cache: 'no-store' })).json();
    const online = !!n.ip;
    wifiRequired = !online;

    if (wifiRequired) {
      $('wifiOn').checked = true;
      $('wifiOn').disabled = true;
      $('wifiFields').classList.remove('hid');
      $('wifiLabel').childNodes[0].nodeValue = 'Wi-Fi details (required)';
      $('wifiHint').innerHTML =
        '<b style="color:var(--warn)">This device cannot reach its stored network.</b> ' +
        'You are connected over its own hotspot, which disappears when it restarts, ' +
        'so a working network is required to finish.';
    } else {
      $('wifiHint').textContent =
        'Connected to ' + n.ip + ' already. Only change this if you want a different network.';
    }
  } catch (e) {
    /* Leave it optional rather than blocking recovery on a failed probe. */
  }
}
checkNetwork();

function msg(t, cls) { const e = $('m'); e.textContent = t; e.className = 'msg ' + (cls || ''); }

$('go').onclick = async () => {
  const a = $('p1').value, b = $('p2').value;
  if (a.length < 4 || a.length > 32) { msg('Password must be 4-32 characters.', 'bad'); return; }
  if (a !== b) { msg('The two passwords do not match.', 'bad'); return; }
  if ($('wifiOn').checked && !$('ssid').value.trim()) {
    msg(wifiRequired
      ? 'A network name is required — this device cannot reach its stored network.'
      : 'Enter the network name, or switch the Wi-Fi change off.', 'bad');
    return;
  }

  $('go').disabled = true;
  msg('Saving…', '');
  try {
    const r = await fetch('/api/recovery/complete', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        password: a,
        ssid: $('wifiOn').checked ? $('ssid').value.trim() : '',
        wifi_pass: $('wifiOn').checked ? $('wpass').value : ''
      })
    });
    if (!r.ok) {
      const j = await r.json().catch(() => ({}));
      throw new Error(j.error || ('HTTP ' + r.status));
    }
    const j = await r.json();
    $('form').classList.add('hid');
    $('done').classList.remove('hid');
    $('where').textContent = j.reconnecting
      ? 'The device is joining the new network now. Reconnect your phone to that network, '
        + 'then find the device there — its hotspot shuts down once it is online.'
      : 'Open the dashboard at ' + (j.url || 'the usual address') + '.';
  } catch (e) {
    msg('Could not save: ' + e.message, 'bad');
    $('go').disabled = false;
  }
};
</script>
</body></html>
)RECOV";
