// ======================= MUTAGEN dentro de FL Studio (VST3) =======================
// El sonido lo hace el plugin (C++). Esta interfaz es la misma de la vista previa: manda el genoma y los mandos,
// las notas del teclado de pantalla y los acordes de prueba, y recibe lo que suena para dibujarlo.
(() => {
  const J = window.__JUCE__ && window.__JUCE__.backend; if (!J) return;
  window.PLUG = true;
  const emit = (n, o) => { try { J.emitEvent(n, o || {}); } catch (e) {} };
  // sin WebAudio: el audio lo pone FL
  ensureAudio = function () { if (actx) return; actx = { resume: () => Promise.resolve(), sampleRate: 48000, state: "running" }; E = { midi: true, mst: { key: null }, det: { st: { key: null } }, voices: [] }; };
  ensureAudio();
  noteOn = function (m, v = 0.8) { emit("note", { m, on: 1, v }); };
  noteOff = function (m) { emit("note", { m, on: 0, v: 0 }); };
  startSource = function () {
    if (srcKind === "acordes") { SEQ.on = true; playing = true; return; }
    SEQ.on = false; playing = true;
    if (srcKind !== "teclado") toast("EN FL STUDIO: TOCA CON TU TECLADO MIDI O PON UN PATRÓN EN EL PIANO ROLL");
  };
  stopSource = function () { SEQ.on = false; playing = false; emit("allOff", {}); };
  const _setSource = setSource;
  setSource = async function (k) { if (k === "user" || k === "piano" || k === "lead") { toast("EN FL STUDIO EL SONIDO ENTRA POR MIDI: TOCA O PON NOTAS EN EL PIANO ROLL"); return; } return _setSource(k); };
  // ---- mandar el genoma y los mandos cuando cambian (para el sonido)
  const ENGP = ["blend", "amb", "oct", "follow", "bright", "attOff", "relOff", "move", "out", "poly", "bpm", "sync", "toca", "vari", "flUp", "flDown", "flFifth"];
  let lastEng = "", lastSave = "", saveT = 0, restored = false;
  const engJson = () => JSON.stringify({ G, P: Object.fromEntries(ENGP.map(k => [k, P[k]])), pg: [PG.gain, PG.tame], seq: [SEQ.on ? 1 : 0, SEQ.prog], bypass: S.bypass ? 1 : 0 });
  const fullJson = () => JSON.stringify({ v: 6, G, P, TREE, MIAS, TASTE, tab: S.tab, cat: S.cat, seqProg: SEQ.prog });
  setInterval(() => {
    if (!restored) return;
    const ej = engJson(); if (ej !== lastEng) { lastEng = ej; emit("eng", { json: ej }); }
    if (++saveT >= 10) { saveT = 0; const fj = fullJson(); if (fj !== lastSave) { lastSave = fj; emit("save", { json: fj }); } }
  }, 50);
  // ---- recuperar lo guardado en el proyecto
  J.addEventListener("restore", o => {
    try {
      if (o && o.json) {
        const st = JSON.parse(o.json);
        if (st.TREE) Object.assign(TREE, st.TREE);
        if (st.G) G = Object.assign(defGenome(), st.G);
        if (st.P) Object.assign(P, st.P);
        if (Array.isArray(st.MIAS)) MIAS = st.MIAS;
        if (Array.isArray(st.TASTE)) TASTE = st.TASTE;
        if (st.tab) S.tab = st.tab; if (st.cat) S.cat = st.cat; if (st.seqProg != null) SEQ.prog = st.seqProg;
        GD.welcome = false;
      }
    } catch (e) {}
    SEQ.on = false; playing = false; restored = true; updatePatchGain(true);
  });
  // ---- lo que suena en FL, para dibujarlo
  J.addEventListener("viz", v => {
    ui.level = v.level || 0; ui.notes = v.notes || []; ui.played = v.played || []; ui.voicesOn = v.voicesOn || 0;
    for (let i = 0; i < 3; i++) ui.roll.push({ heard: ui.notes, played: ui.played.slice() }); while (ui.roll.length > ROLLN) ui.roll.shift();
    E.mst.key = v.key ? { k: v.key[0], minor: !!v.key[1], c: 1 } : null;
    if (v.bpm) P.bpm = v.bpm;
    if (ui.notes.length || SEQ.on) playing = true;
  });
  emit("ready", {});
})();
