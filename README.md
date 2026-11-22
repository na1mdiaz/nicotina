# PEPELUS SATIVA OPERATIONS · plugins VST3

Tres plugins de una o dos perillas con escena animada de Pepelu:
SATURA (saturador), SLOWED (slowed + reverb + tape stop) y NINJA (delay creativo de 12 estilos).

- `Source/*/…Engine.h`: el sonido, traducción exacta de las vistas previas web (comprobado muestra a muestra).
- `ui/*/index.html`: la interfaz (la misma de la vista previa, con puente al plugin).
- `tools/Render.cpp`, `tools/Stress.cpp`: pruebas offline.

Se compila solo en GitHub Actions (rama `pepelus`): JUCE 9 + WebView2.
