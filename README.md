# Trance Module XR-1

Modulo de sonidos en rack (sampler) con interfaz amarilla. VST3 para FL Studio y cualquier DAW.

- 50 sonidos incrustados: Leads, Brass, Bells, Pulsatings, Seq (se hace clic en la pantalla LCD para elegir)
- 16 voces, interpolacion Hermite, loop con crossfade (boton LOOP)
- Amp ADSR, filtro (LP/HP/BP) con envolvente, tune/fine, delay ping-pong, reverb, volumen
- Pitch bend de +-2 semitonos. Nota raiz de los samples: C5 de FL Studio (Do central, MIDI 60)

## Compilar en Windows (opcion A: sin instalar nada, con GitHub)

1. Crea un repositorio en GitHub y sube todo el contenido de esta carpeta.
2. Pestana **Actions** -> "Build VST3 (Windows)" -> espera ~10-20 min.
3. Descarga el artifact `Trance-Module-XR-1-VST3-Windows`.
4. Copia la carpeta `Trance Module XR-1.vst3` a `C:\Program Files\Common Files\VST3\`.
5. En FL Studio: Options -> Manage plugins -> *Find installed plugins*.

## Compilar en Windows (opcion B: local)

Requisitos: Visual Studio 2022 (carga de trabajo "Desarrollo de escritorio con C++"), CMake y Git.

    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
    cmake --build build --config Release --target TranceModuleXR1_VST3

El `.vst3` queda dentro de `build/TranceModuleXR1_artefacts/Release/VST3/`.
La primera compilacion descarga JUCE 8.0.4 y tarda un buen rato (los samples estan incrustados).

## Licencia de JUCE

JUCE es AGPLv3 o de pago. Para distribuir o vender el plugin hay que cumplir una de las dos:
publicar el codigo bajo AGPLv3, o tener una licencia de JUCE (la gratuita "Personal" muestra una pantalla de presentacion).

## Samples

`Samples/` contiene los one-shots originales ya convertidos (FLAC 16 bits, 44.1 kHz, normalizados).
Para agregar o cambiar sonidos: poner archivos `Categoria_NN_Nombre.flac` (categorias: Leads, Brass, Bells, Pulsatings, Seq) y recompilar.
