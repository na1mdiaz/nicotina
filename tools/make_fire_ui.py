# Convierte la vista previa de PEPELUS FIRE (index.html con fuentes) en la interfaz del plugin.
# Uso: python3 tools/make_fire_ui.py /ruta/a/vista-previa.html
import sys
src = sys.argv[1] if len(sys.argv) > 1 else "/home/claude/index.html"
s = open(src, encoding="utf-8").read()
def r(old, new, cnt=1):
    global s
    assert s.count(old) == cnt, (s.count(old), old[:80]); s = s.replace(old, new)

r('"use strict";', '"use strict";\nconst IS_PLUGIN = !!(window.__JUCE__ && window.__JUCE__.backend);')
# sin WebAudio dentro del plugin: búferes propios y reloj propio (el sonido sale por el DAW)
r('function ensureAudio() { if (!actx) actx = new (window.AudioContext || window.webkitAudioContext)(); actx.resume(); }',
  '''function FakeBuf(ch, len, sr) { this.numberOfChannels = ch; this.length = len; this.sampleRate = sr; this.duration = len / sr; this._d = []; for (let c = 0; c < ch; c++) this._d.push(new Float32Array(len)); }
FakeBuf.prototype.getChannelData = function (c) { return this._d[Math.min(c, this._d.length - 1)]; };
function ensureAudio() { if (!actx) actx = IS_PLUGIN ? { sampleRate: 48000, get currentTime() { return performance.now() / 1000; }, resume() {}, createBuffer: (ch, len, sr) => new FakeBuf(ch, len, sr) } : new (window.AudioContext || window.webkitAudioContext)(); actx.resume(); }''')
r('const S = { tab: 1, mode: "STUTTER",', 'const S = { sync: IS_PLUGIN, tab: 1, mode: "STUTTER",')
# botón SYNC (sigue el tempo del DAW) y tocar el BPM a mano quita el SYNC
r('  btn("tap", "TAP", 560, 12, 44, 22,', '  if (IS_PLUGIN) btn("sync", "SYNC", 506, 12, 48, 22, S.sync, { font: F2.mono(12) });\n  btn("tap", "TAP", 560, 12, 44, 22,')
r('  const h = hitAt(p); if (!h) return; cvs.setPointerCapture(e.pointerId);',
  '  const h = hitAt(p); if (!h) return; cvs.setPointerCapture(e.pointerId);\n  if (h.id === "sync") { S.sync = !S.sync; toast(S.sync ? "SYNC: SIGUE EL TEMPO DEL DAW" : "SYNC OFF: TEMPO A MANO"); return; }\n  if (h.id === "bpm-" || h.id === "bpm+" || h.id === "bpmv" || h.id === "tap") S.sync = false;')
r('if (h && h.id === "bpmv") { e.preventDefault();', 'if (h && h.id === "bpmv") { e.preventDefault(); S.sync = false;')

bridge = r'''
// ------------------------------------------------------------------ puente con el plugin (VST3 / AU)
if (IS_PLUGIN) {
  const BK = window.__JUCE__.backend;
  const b64 = u8 => { let s = ""; for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode.apply(null, u8.subarray(i, i + 0x8000)); return btoa(s); };
  const i16b64 = (L, R) => { const v = new Int16Array(L.length * 2); for (let i = 0; i < L.length; i++) { v[i * 2] = Math.max(-32767, Math.min(32767, Math.round(L[i] * 32767))); v[i * 2 + 1] = Math.max(-32767, Math.min(32767, Math.round(R[i] * 32767))); } return b64(new Uint8Array(v.buffer)); };
  const wavU8 = (L, R, fs) => { const n = L.length, data = n * 6, buf = new ArrayBuffer(44 + data), v = new DataView(buf); let o = 0;
    const st = t => { for (let i = 0; i < t.length; i++) v.setUint8(o++, t.charCodeAt(i)); }, u32 = x => { v.setUint32(o, x, true); o += 4; }, u16 = x => { v.setUint16(o, x, true); o += 2; };
    st("RIFF"); u32(36 + data); st("WAVE"); st("fmt "); u32(16); u16(1); u16(2); u32(fs); u32(fs * 6); u16(6); u16(24); st("data"); u32(data);
    for (let i = 0; i < n; i++) for (const x of [L[i], R[i]]) { const q = Math.max(-8388608, Math.min(8388607, Math.round(Math.max(-1, Math.min(1, x)) * 8388607))); v.setUint8(o++, q & 255); v.setUint8(o++, (q >> 8) & 255); v.setUint8(o++, (q >> 16) & 255); }
    return new Uint8Array(buf); };
  // el clip viaja al plugin después de cada cambio (y antes de escucharlo o arrastrarlo)
  let clipId = 0, sentId = -1, sendT = 0;
  const sendClip = () => { if (!clip || sentId === clipId) return; sentId = clipId; BK.emitEvent("clip", { name: clipName, wav: b64(wavU8(clip.L, clip.R, clip.fs)) }); };
  const _doRegen = doRegen;
  doRegen = function () { _doRegen(); if (clip) { clipId++; clearTimeout(sendT); sendT = setTimeout(sendClip, 250); } };
  playBuf = function (L, R, fs, kind) {
    stopPlay();
    if (kind === "clip") { sendClip(); BK.emitEvent("play", { kind: "clip" }); } else BK.emitEvent("play", { kind: "sel", sr: fs, data: i16b64(L, R) });
    const t0 = actx.currentTime + 0.04;
    S.prev = { kind, t0, dur: L.length / fs, L, fs, shots: 0, word: kind === "clip" ? clip.word / fs : 0, hits: kind === "clip" ? clip.hits.map(h => h / fs) : [], bv: kind === "clip" ? clip.bvEnd / fs : 0, puffed: false };
    setTimeout(() => { if (S.prev && S.prev.t0 === t0) S.prev = null; }, (L.length / fs + 0.15) * 1000);
  };
  stopPlay = function () { BK.emitEvent("stop", {}); S.prev = null; };
  toggleRec = async function () { ensureAudio(); S.rec = !S.rec; BK.emitEvent("rec", { on: S.rec }); if (S.rec) { S.recT = performance.now(); toast("GRABANDO LA PISTA: DALE A PLAY · PULSA OTRA VEZ PARA PARAR"); } };
  saveClip = async function () { if (!clip) return; sendClip(); BK.emitEvent("save", {}); };
  document.getElementById("file").click = () => BK.emitEvent("open", {});
  // arrastrar: el plugin crea el .wav y lo arrastra al DAW (FL, Ableton, Logic, Cubase...)
  dragEl.setAttribute("draggable", "false");
  dragEl.addEventListener("pointerdown", e => { if (!clip || e.button !== 0) return; sendClip(); BK.emitEvent("drag", {}); });
  dragEl.title = "Arrastra el clip a tu DAW";
  BK.addEventListener("source", o => { const n = o.n | 0, bin = atob(o.data), u8 = new Uint8Array(bin.length); for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i);
    const v = new Int16Array(u8.buffer, 0, Math.floor(u8.length / 2)), m = Math.min(n, v.length >> 1), b = new FakeBuf(2, Math.max(1, m), o.sr || 48000), L = b.getChannelData(0), R = b.getChannelData(1);
    for (let i = 0; i < m; i++) { L[i] = v[i * 2] / 32768; R[i] = v[i * 2 + 1] / 32768; }
    S.rec = false; setSource(b, o.name || "AUDIO"); });
  BK.addEventListener("toast", m => toast(String(m)));
  let lastJ = "";
  BK.addEventListener("state", o => { try { if (o && o.json) { Object.assign(PAR, JSON.parse(o.json)); lastJ = o.json; regen(); } } catch (e) {} });
  BK.addEventListener("viz", v => { S.hostPlaying = !!v.playing; if (S.rec && !v.rec) S.rec = false;
    if (S.sync && v.bpm > 20) { const b = Math.max(60, Math.min(200, Math.round(v.bpm))); if (b !== PAR.bpm) { PAR.bpm = b; regen(); } } });
  setInterval(() => { const j = JSON.stringify(PAR); if (j !== lastJ) { lastJ = j; BK.emitEvent("state", { json: j }); } }, 800);
  ensureAudio();
  BK.emitEvent("ready", {});
}
'''
i = s.rindex("</script>")
s = s[:i] + bridge + s[i:]
open("ui/fire/index.html", "w", encoding="utf-8").write(s)
print("ui/fire/index.html", len(s))
