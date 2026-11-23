# Interfaz del VST3 = la vista previa 6.0 (con sus fuentes) + el puente con el plugin
import re
src = open("/home/claude/mutagen5/index.html", encoding="utf-8").read()
ad = open("/home/claude/mutagen6/tools/adapter.js", encoding="utf-8").read()
i = src.rindex("</script>")
out = src[:i] + "\n" + ad + "\n" + src[i:]
open("/home/claude/mutagen6/ui/index.html", "w", encoding="utf-8").write(out)
print("ui", len(out))
