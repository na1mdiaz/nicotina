# Convierte cada vista previa (index.html ya con fuentes) en la interfaz del plugin.
import re, sys
SRC = {"satura": "/home/claude/pota/index.html", "slowed": "/home/claude/cueva/index.html", "ninja": "/home/claude/ninja/index.html"}
BRIDGE = {
"satura": dict(params={"k": "S.k", "mix": "S.mix", "preset": "S.preset", "bypass": "S.bypass"},
               viz="S.inL = v.inL; S.outL = v.outL; S.lv = v.lv; DSP.gr = v.gr; S.louder = v.louder;"),
"slowed": dict(params={"k": "S.k", "mix": "S.mix", "preset": "S.preset", "bypass": "S.bypass"},
               viz="S.inL = v.inL; S.outL = v.outL; S.lv = v.lv; DSP.gr = v.gr; S.tv = v.tv; S.tape = v.tape; S.st = v.st;"),
"ninja":  dict(params={"k": "S.k", "div": "S.div", "preset": "S.preset", "bypass": "S.bypass"},
               viz="S.inL = v.inL; S.outL = v.outL; DSP.echo = v.echo; DSP.hold = v.hold; DSP.ticks = v.ticks; DSP.frozen = v.frozen; if (v.bpm) S.bpm = Math.round(v.bpm);",
               extra="if (k === 'preset') S.bank = Math.floor(val / 6);"),
}
for name, path in SRC.items():
    s = open(path, encoding="utf-8").read()
    b = BRIDGE[name]
    # 1) saber si estamos dentro del plugin
    s = s.replace('"use strict";', '"use strict";\nconst IS_PLUGIN = !!(window.__JUCE__ && window.__JUCE__.backend);', 1)
    # 2) en el plugin no hay WebAudio: el sonido lo hace FL
    s = s.replace("let actx = null,", "let actx = IS_PLUGIN ? { resume() {}, sampleRate: 48000, plugin: true } : null,", 1)
    s = re.sub(r"function play\(kind\) \{", "function play(kind) {\n  if (IS_PLUGIN) return;", s, count=1)
    # 3) sin botones de sonidos de prueba: en su lugar, lo que suena en FL
    lines = s.split("\n"); out = []
    for ln in lines:
        st = ln.strip()
        if st.startswith('txt("PRUEBA CON"'):
            out.append("  if (IS_PLUGIN) { txt(\"EN FL STUDIO\", 14, " + ("bd.y" if name != "ninja" else "by") + " + 24, F.pix(16), \"#5d5c57\"); txt(\"SUENA LO QUE PASE\", 14, " + ("bd.y" if name != "ninja" else "by") + " + 54, F.pix(18), C.bone); txt(\"POR ESTE CANAL\", 14, " + ("bd.y" if name != "ninja" else "by") + " + 74, F.pix(18), C.bone); txt(\"TEMPO DEL PROYECTO · \" + Math.round(PLUGIN_BPM) + \" BPM\", 14, " + ("bd.y" if name != "ninja" else "by") + " + 100, F.pix(16), C.dim); } else {")
            out.append(ln); continue
        if (st.startswith('[["voz"') or st.startswith('[["caja"')) and "btn(\"src_\"" in st:
            out.append(ln); out.append("  }"); continue
        if name == "ninja" and st.startswith('txt("BPM " + S.bpm'):
            out.append('  if (IS_PLUGIN) txt("BPM " + S.bpm + " (FL)", 506, by + 99, F.pix(17), C.dim); else {'); out.append(ln); out.append("  }"); continue
        out.append(ln)
    s = "\n".join(out)
    # 4) puente con el plugin
    pmap = ", ".join(f'{k}: {{ get: () => +({v}), set: val => {{ {v} = {"val > 0.5" if k == "bypass" else "val"}; }} }}' for k, v in b["params"].items())
    bridge = f"""
// ------------------------------------------------------------------ puente con el plugin (FL Studio)
let PLUGIN_BPM = 0;
if (IS_PLUGIN) {{
  const BK = window.__JUCE__.backend, PM = {{ {pmap} }}, last = {{}}; let got = false;
  S.playing = true;
  BK.addEventListener("params", o => {{ for (const k in o) if (PM[k]) {{ const val = +o[k]; PM[k].set(val); last[k] = PM[k].get(); {b.get("extra", "")} }} got = true; }});
  BK.addEventListener("viz", v => {{ {b["viz"]} if (v.bpm) PLUGIN_BPM = v.bpm; S.playing = true; }});
  setInterval(() => {{ if (!got) return; const ch = {{}}; let any = false; for (const k in PM) {{ const val = PM[k].get(); if (last[k] === undefined || Math.abs(val - last[k]) > 1e-6) {{ ch[k] = val; last[k] = val; any = true; }} }} if (any) BK.emitEvent("setParams", ch); }}, 30);
  BK.emitEvent("ready", {{}});
}}
"""
    i = s.rindex("</script>")
    s = s[:i] + bridge + s[i:]
    open(f"/home/claude/pepelus-vst/ui/{name}/index.html", "w", encoding="utf-8").write(s)
    print(name, len(s))
