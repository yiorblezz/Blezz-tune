# BLEZZ TUNE V2 — PSOLA Vocal Suite Trap (VST3 para FL Studio / Windows)

Plugin de voz todo-en-uno, negro y morado:

1. **AUTO TUNE (motor PSOLA nuevo)** — afinación sincronizada con cada ciclo de la voz: limpia, sin "phasing" y sin efecto ardilla.
   - **Modo Estudio**: máxima calidad (≈33 ms de retraso que FL compensa solo al reproducir).
   - **Modo Tiempo Real**: para grabar escuchándote con el autotune (≈12–15 ms).
   - Retune Speed, Humanize, **Flex-Tune** (deja pasar notas lejanas = más natural) y **Vibrato** (conserva tu vibrato aunque el retune sea rápido).
   - Gráfico de tono en vivo: tu voz (gris) vs. afinada (morado).
2. **FET 76** — compresor rápido estilo 1176 (Input, Output, Attack, Release, Ratio 4/8/12/20/ALL).
3. **OPTO 2A** — compresor óptico suave estilo LA-2A (Peak Reduction, Gain, Compress/Limit).
4. **BRILLO** — una barra que sube presencia y aire a la voz.

Orden de la cadena: Tune → FET 76 → OPTO 2A → Brillo (la cadena clásica de estudio).

La V2 se instala como un plugin aparte (**BLEZZ TUNE V2**), así tus proyectos con la V1 siguen funcionando.

## Cómo conseguir el archivo .vst3

### Opción A — sin instalar nada (GitHub, gratis)
1. Crea una cuenta en github.com y un repositorio nuevo.
2. Sube **todo el contenido de esta carpeta** (incluida la carpeta oculta `.github`). Con GitHub Desktop es arrastrar y soltar.
3. Entra a la pestaña **Actions** del repositorio: la compilación arranca sola (unos 10 minutos).
4. Cuando termine en verde, abre la ejecución y descarga **BLEZZ-TUNE-VST3-Windows**.
5. Descomprime y copia la carpeta `BLEZZ TUNE.vst3` a `C:\Program Files\Common Files\VST3\`.

### Opción B — compilar en tu PC
Instala Visual Studio 2022 Community (marca "Desarrollo para el escritorio con C++"), CMake y Git. Luego doble clic en `build_windows.bat`.

### En FL Studio
Options → Manage plugins → **Find plugins**. Aparece como **BLEZZ TUNE V2**. Ponlo en el primer slot del canal de la voz en el Mixer.

## Uso rápido
- Pon la **Tonalidad** y **Escala** del beat (si no la sabes, prueba con *Cromática*).
- **Retune Speed 0 ms** = efecto robot / hard tune. **15–30 ms** = afinado natural.
- **Humanize** deja respirar las notas largas (vibrato natural).
- Elige un **Preset** para partir y ajusta a gusto. Los presets NO cambian la tonalidad.

## Presets
| Preset | Escala | Retune | Humanize | Flex | Vibrato | Brillo |
|---|---|---|---|---|---|---|
| Default Natural | Menor | 20 ms | 25% | 25% | 40% | 35% |
| Estilo Bad Bunny | Menor | 10 ms | 15% | 10% | 20% | 55% |
| Estilo Anuel AA (Hard Tune) | Menor | 0 ms | 0% | 0% | 0% | 50% |
| Estilo Myke Towers | Menor | 4 ms | 10% | 0% | 10% | 50% |
| Estilo Ozuna (Melódico) | Menor | 2 ms | 5% | 0% | 15% | 60% |
| Estilo Rauw Alejandro (R&B) | Menor | 18 ms | 35% | 30% | 60% | 65% |
| Estilo Feid (Suave) | Menor | 12 ms | 25% | 20% | 35% | 60% |
| Estilo Karol G (Pop Urbano) | Mayor | 25 ms | 40% | 35% | 60% | 60% |
| Estilo Eladio Carrión (Trap) | Menor | 1 ms | 5% | 0% | 5% | 45% |
| Estilo Duki (Trap AR) | Menor | 0 ms | 0% | 0% | 0% | 50% |
| Dembow Frigio | Frigia | 6 ms | 10% | 0% | 10% | 55% |
| Robot 777 | Cromática | 0 ms | 0% | 0% | 0% | 45% |

Los presets son aproximaciones de oído al sonido de esas voces en sus discos, no configuraciones oficiales de los artistas. Cada preset también trae su ajuste de FET 76 y OPTO 2A.

## Nota
BLEZZ TUNE es un plugin original. No contiene código de Antares, Universal Audio ni Waves; los nombres "estilo 1176 / LA-2A / Auto-Tune" describen el tipo de procesamiento.
