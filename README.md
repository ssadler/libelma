
# libelma

*WARNING*

Here be dragons! This is a slapshod fork of elmadev/elma-classic, made to work as a library so it can be scripted, and with a work-in-progress OpenGL renderer.

`libelma` is:

* A `.so` shared object build of Elasto Mania suitable for TAS and scripting (see `elma/lib.h` and `elma/lib_gameloop.h`).
* A Node.JS addon so that scripts can be written in JS (see `binding.cc`).
* A Typescript application with saveload functionality (see `src/main.ts`).

## Configuration

You will need to edit .env.local:

```bash
ELMA_DIR="path/to/your/elma"
# Optional settings.json as from elmadev/elma-classic:
SETTINGS_JSON="path/to/settings.json"
```

## Building

Dependencies:

* A recent version of node.js and NPM.
* Dependencies from `elma/docs/BUILDING.md`.

```bash
./manage.sh build_all
```

## Playing

```bash
./manage.sh main # Loads internal levels
# or
./manage.sh main [... .lev files in lev/ dir without path or zero indexed int. number]
```

See src/main.ts to figure out controls for saveload, or change them. Similar to okesl.

### Controls

* up: gas
* down: brake
* left: volt left
* right: volt right
* space: turn
* rctrl: alo
* enter: pause
* s: save
* l: load
* r: reset
* /: speed 1
* ': speed .6
* lshift+': speed .3
* ;: speed .15
* lshift+;: speed .075
* [: seek back      (lctrl for slower, lshift for faster)
* ]: seek forward
* +/=: zoom in
* -: zoom out
* q: toggle quality
* lshift+esc: quit (also lctrl+q)
* lctrl+n: next level
* lctrl+p: prev level

When paused:

* up: unpause & clear auto
* down: unpause & clear auto
* backspace: clear auto
* n: next frame
* b: back frame
* lctrl+up/down: select input event
* left/right: adjust input event frame


## Developing

You should `export LD_LIBRARY_PATH="./elma/build/"`

Or run any command via `manage.sh` i.e. `./manage.sh [your shell command]`


## License

See [elma/LICENSE.md](elma/LICENSE.md)
