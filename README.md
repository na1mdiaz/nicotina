# MUTAGEN 変異

Efecto para sintes, leads y pads: secuenciador tipo Gross Beat, 8 distorsiones, 8 filtros,
granos con voces/estirar/delay de granos, rack de efectos, máster, 2 curvas dibujables + seguidor + ejes XY
asignables a cualquier mando, y genéticas que se cruzan y mutan.

- `Source/Engine.h` — motor de audio (C++, sin dependencias).
- `Source/PluginProcessor.*` — parámetros automatizables (JUCE) y guardado de estado.
- `Source/PluginEditor.*` — interfaz web (WebView2 / WKWebView) conectada al motor.
- `ui/index.html` — la interfaz.
- `tools/Render.cpp` — render offline para pruebas (`-DMUTAGEN_TOOLS=ON -DMUTAGEN_PLUGIN=OFF`).

Compila con CMake + JUCE 9 (se descarga solo). Ver `.github/workflows/build.yml`.
