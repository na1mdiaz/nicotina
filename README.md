# NICOTINA ニコチン

**8-BIT CRUSH SYSTEM** — bitcrusher VST3 (y AU en Mac) con interfaz de consola portátil: pantalla LCD de 4 tonos donde una mano tatuada apaga un cigarro según el CRUSH. A partir del 90 % lo aplasta del todo, y cada golpe de la música suma combos.

Hecho por **na1mdiaz** con [JUCE](https://juce.com) 9.

## Qué hace

| Mando | |
|---|---|
| **CRUSH** | Macro principal: de limpio a los valores de Bit Depth / Sample Rate, con saturación y volumen compensado |
| **BIT DEPTH** | 1–16 bits |
| **SAMPLE RATE** | 1/1 – 1/64 |
| **DRY/WET** | Mezcla en paralelo |
| **OUTPUT** | -24 a +12 dB |

**Modos:** GAMEBOY · SP-1200 · RADIO · DESTROY
**14 presets** en el cartucho (y en el menú de presets del DAW): INIT, 808 SUCIO, 808 RABIA, KICK PUNCH, SNARE CRUJE, HATS METAL, ONI 鬼, VOZ ROBOT, RADIO ROTA, CHIPTUNE, PAD LOFI, BAJO REESE, CINTA VIEJA, APLASTADO.
**Paletas:** DMG, POCKET, YAKUZA, NEON (botón SELECT).

La pantalla reacciona al sonido: menos bits = menos tonos, menos sample rate = píxeles más gordos.

## Descargar

Pestaña **Actions** → última ejecución verde → **Artifacts** → `NICOTINA-Windows` / `NICOTINA-macOS`.
Para publicar una versión: `git tag v1.0.0 && git push --tags` y aparece en **Releases**.
Instrucciones de instalación en `INSTALAR.txt`.

## Vista previa

`docs/vista-previa.html` es la misma interfaz y el mismo sonido funcionando en el navegador.

## Compilar a mano

```bash
cmake -S . -B build
cmake --build build --config Release
```

Necesita CMake 3.22+ y Visual Studio 2022 (Windows) o Xcode (Mac). El plugin queda en `build/Nicotina_artefacts/Release/VST3/`.

## Licencias

- JUCE: AGPLv3 o licencia comercial (la licencia gratuita "Starter" de JUCE sirve mientras los ingresos sean bajos; revisa juce.com antes de venderlo).
- Fuentes Press Start 2P, Silkscreen, DotGothic16, Yuji Syuku e Inter: SIL Open Font License (`assets/fonts/OFL.txt`).
