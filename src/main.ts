
// @ts-ignore
import elma from 'elma.node'
import path from 'path'
import * as process from 'process'
import { globSync } from 'glob'


const FPS = 450


async function main() {
  process.chdir(process.env.ELMA_DIR!)

  let args = process.argv.slice(2)
  let levs: (string | number)[] = []

  if (args.length == 0) {
    // load internal levs
    levs = new Array(54).fill(0).map((_, i) => i)
  } else {

    levs = args.flatMap((a) => {
      if (String(parseInt(a)) == a) {
        return [parseInt(a)] as any
      }
      let m = globSync(`./lev/${a}`)
      if (m.length == 0) {
        console.warn(`No levels matching ${a}`)
        process.exit(1)
      }
      return m.map((l) => path.basename(l))
    })
  }


  elma.elmaInit();
  elma.setQuality(1);
  elma.setDT(0.182 * .0024 * (1000 / FPS))


  for (let l=0; ;) {
    let lev = levs[l%levs.length]
    let r = await gameloop(lev!)
    if (r == 'next') {
      l++
    } else if (r == 'prev') {
      l = levs.length + l - 1
    } else {
      break
    }
  }
}

async function gameloop(lev: string | number) {
  console.log("lev", lev)

  let frame = 0
  let slots = new Slots(lev, elma.initGame(lev))
  let paused = false
  let frameEdit = 0

  elma.setGLRenderCallback(() => renderTAS(slots, paused, frame, frameEdit))

  function render() {
    frame = slots.slot.frame
    elma.render(slots.slot.sim) //, [slots.slots[1]!.sim])
  }

  let _speed = 1
  let lastRender = timing()

  elma.handleEvents()

  for (let l=0; ; l++) {

    let speed = _speed
    let doRender = l % 30 == 0 // render but slower when paused

    let done = slots.sim.dead() || slots.sim.finished()
    const lctrl = elma.keyIsDown(SSC.LCTRL)
    const lshift = elma.keyIsDown(SSC.LSHIFT)

    if (lctrl && elma.keyJustPressed(SSC.N)) {
      return "next"
    } else if (lctrl && elma.keyJustPressed(SSC.P)) {
      return "prev"
    }

    if (elma.keyJustPressed(SSC.S)) {
      slots.save()
    } else if (elma.keyJustPressed(SSC.L)) {
      paused = true
      slots.load()
      render()
    }

    if (!lctrl) {
      if (elma.keyJustPressed(SSC.UP) || elma.keyJustPressed(SSC.DOWN)) {
        slots.slot.truncate()
        paused = false
        frameEdit = 0
      }
    }

    if (lctrl && elma.keyJustPressed(SSC.Q)) process.exit()
    if (lshift && elma.keyJustPressed(SSC.ESCAPE)) process.exit()
    if (!lctrl && elma.keyJustPressed(SSC.RETURN)) {
      paused = !paused
      frameEdit = 0
    }

    if (paused && elma.keyJustPressed(SSC.N)) {
      slots.slot.step()
      slots.save()
      render()
    }
    if (!lctrl && paused && elma.keyJustPressed(SSC.B)) {
      slots.slot.back()
      slots.save()
      render()
    }
    if (!lctrl && elma.keyJustPressed(SSC.R)) {
      paused = true
      slots.slot.reset()
      render()
    }
    if (!lctrl && elma.keyJustPressed(SSC.BACKSPACE)) slots.slot.truncate()

    if (elma.keyJustPressed(SSC.APOSTROPHE)) {
      speed = _speed = lshift ? .6 : .3
    } else if (elma.keyJustPressed(SSC.SEMICOLON)) {
      speed = _speed = lshift ? .075 : .15
    } else if (elma.keyJustPressed(SSC.SLASH)) {
      speed = _speed = 1
    } else {
      let lbrac = elma.keyIsDown(SSC.LEFTBRACKET)
      let rbrac = elma.keyIsDown(SSC.RIGHTBRACKET)
      if (lbrac || (rbrac && !done)) {
        paused = true;
        let n = lctrl ? 1 : lshift ? 12 : 4
        lbrac ? slots.slot.back(n) : slots.slot.step(n)
        if (l % (lshift?3:1) == 0) render()
        speed = lctrl ? .3 : 1
      }
    }

    if (paused) {
      let events = slots.slot.getKeyEvents(8)
      if (events.length) {
        let [f, k] = events[frameEdit]!
        if (elma.keyIsDown(SSC.LEFT)) {
          if (!lctrl || elma.keyJustPressed(SSC.LEFT)) {
            if (f > 0) {
              events.push([f-1, k])
              slots.slot.patch(events)
            }
            doRender = true
          }
        } else if (elma.keyIsDown(SSC.RIGHT)) {
          if (!lctrl || elma.keyJustPressed(SSC.RIGHT)) {
            events[frameEdit]![0]++
            if (frameEdit < 7) {
              events.push([f, events[frameEdit+1]![1]])
            }
            slots.slot.patch(events)
            doRender = true
          }
        } else if (lctrl && elma.keyJustPressed(SSC.UP)) {
          frameEdit = Math.max(0, frameEdit-1)
          doRender = true
        } else if (lctrl && elma.keyJustPressed(SSC.DOWN)) {
          frameEdit = Math.min(7, frameEdit+1)
          doRender = true
        }
      }
    }

    if (!lctrl && elma.keyJustPressed(SSC.Q)) {
      elma.setQuality(101)
    }
    
    if (elma.keyIsDown(SSC.MINUS)) {
      elma.adjustZoom(-.01)
      doRender = true
    }
    if (elma.keyIsDown(SSC.EQUALS)) {
      elma.adjustZoom(+.01)
      doRender = true
    }

    if (!paused && !done) {
      slots.slot.step()
      doRender = true

      if (slots.slot.sim.dead() || slots.slot.sim.finished()) {
        paused = true
      }
    }


    let ms = (1000 / FPS) / speed
    let now = timing()
    let delay = (lastRender + ms) - now
    if (delay > 0) elma.sleep(delay)
    doRender && render()
    lastRender = delay > 0 ? (lastRender+ms) : now


    elma.handleEvents()
  }
}

import fs from 'fs'

class Slots {
  current: number
  slots: Slot[]
  saves: Slot[]
  constructor(public lev: string | number, sim: any) {
    this.slots = [new Slot(sim)] // , new Slot(sim.copy(), true)]
    this.current = 0
    this.saves = []
  }
  save() {
    this.saves[this.current] = this.slots[this.current]!.copy()
    this.slot.write(this.lev)
  }
  load() {
    if (this.saves[this.current]) {
      this.slots[this.current] = this.saves[this.current]!.copy()
    } else {
      this.slot.load(this.lev)
    }
  }
  get slot() {
    return this.slots[this.current]!
  }
  get sim() {
    return this.slot.sim
  }
}







const CHECKPOINT_INTERVAL = 30


class Slot {
  frame: number
  keys: number[]
  checkpoints: any[]
  lastpos: { x: number, y: number } | undefined
  lastspeed = 0

  constructor(public sim: any, public shadow=false) {
    this.sim = sim
    this.frame = 0
    this.keys = []
    this.checkpoints = [sim.copy()]
  }

  copy() {
    let slot = new Slot(this.sim.copy())
    slot.frame = this.frame
    slot.keys = this.keys.slice()
    slot.checkpoints = this.checkpoints.slice()
    return slot
  }

  step(n=1) {
    for (let i=0; i<n; i++) {
      let keys = 0

      if (this.frame < this.keys.length) {
        keys = this.keys[this.frame]!
      } else if (!this.shadow) {

        // Add variables at bottom of file to make configurable
        let alo = elma.keyIsDown(SSC.RCTRL)
        keys = (
          (elma.keyIsDown(SSC.UP) ? 1 : 0) +
          (elma.keyIsDown(SSC.DOWN) ? 2 : 0) +
          (elma.keyIsDown(SSC.LEFT) || alo ? 4 : 0) +
          (elma.keyIsDown(SSC.RIGHT) || alo ? 8 : 0) +
          (elma.keyJustPressed(KEY_TURN) ? 16 : 0)
        )
        this.keys[this.frame] = keys
      }

      if (this.frame % CHECKPOINT_INTERVAL == 0) {
        this.checkpoints[this.frame / CHECKPOINT_INTERVAL] = this.sim.copy()
      }

      elma.step(this.sim, keys, [])
      this.frame++
      this._updateSpeed()
    }
  }

  back(n=1) {

    for (let j=0; j<n; j++) {

      if (this.frame == 0) return
      this.frame--

      let checkidx = Math.floor(this.frame / CHECKPOINT_INTERVAL)
      let off = this.frame % CHECKPOINT_INTERVAL
      let check = this.checkpoints[checkidx]
      this.sim = check.copy()
      for (let i=0; i<off; i++) {
        let f = checkidx * CHECKPOINT_INTERVAL + i
        let keys = this.keys[f]
        this.sim.step(keys)
        this._updateSpeed()
      }
    }
  }
  _updateSpeed() {
      let { x, y } = this.sim.body_r()
      if (this.lastpos) {
        let hyp = Math.sqrt((x-this.lastpos.x)**2 + (y-this.lastpos.y)**2)
        this.lastspeed = hyp * FPS
      }
      this.lastpos = { x, y }
  }

  reset() {
    this.sim = this.checkpoints[0].copy()
    this.checkpoints.length = 1
    this.keys = []
    this.frame = 0
  }

  get keysRendered() {
    return renderKeys(this.keys[this.frame-1] || 0)
  }

  truncate() {
    this.keys.length = this.frame
  }

  getKeyEvents(n=5): [number, number][] {
    let last = -1
    let events: any = []
    for (let i=0; i<this.keys.length; i++) {
      let k = this.keys[i]!
      if (k != last) {
        events.push([i, k])
        last = k
      }
    }
    return events.reverse().slice(0, n)
  }
  patch(events: [number, number][]) {
    let diff = this.frame - events[0]![0]!
    //this.back(this.frame)
    for (let [f, k] of events) {
      this.keys[f] = k
    }
    this.sim = this.checkpoints[0].copy()
    this.checkpoints.length = 1
    let f = this.frame
    this.frame = 0
    this.step(f)
  }
  get status() {
    if (this.sim.dead()) return "DEAD"
    if (this.frame < this.keys.length) return "AUTO"
    return "RUNNING"
  }
  write(lev: string | number) {
    if (!fs.existsSync("libelma_saves")) {
      fs.mkdirSync("libelma_saves")
    }
    let data = JSON.stringify({
      lev,
      frame: this.frame,
      fps: FPS,
      time: new Date().toISOString(),
      keys: this.keys,
    })
    fs.writeFileSync(`libelma_saves/${lev}.save.json`, data)
  }
  load(lev: string | number) {
    let f = `libelma_saves/${lev}.save.json`
    if (fs.existsSync(f)) {
      let data = JSON.parse(fs.readFileSync(f, 'utf8'))
      this.keys = data.keys
      this.checkpoints = [this.checkpoints[0]]
      this.step(data.frame)
    }
  }
}

function renderTAS(slots: Slots, paused: boolean, frame: number, frameEdit: number) {
  let f = paused ? frame : frame - (frame % 7)
  let mins = String(Math.floor(frame / FPS / 60)).padStart(2, '0')
  let secs = String(Math.floor((frame / FPS) % 60)).padStart(2, '0')
  let ms = String(Math.floor((f / FPS * 100) % 100)).padStart(2, '0')
  let time = `${mins}:${secs}:${ms}`

  let head = `TIME        FRAME   SPEED   KEYS   STATUS`
  let slot0 = `${time}   ` +
    `${String(f).padStart(6, ' ')}  ` +
    `${(slots.slot.lastspeed * 10).toFixed(2).padStart(6)}   ` +
    `${slots.slot.keysRendered}  `

  let a = .2

  elma.renderBox(0, 0, 0, a, 10, 10, 2, head.length + 2, 15, 18)
  elma.renderText(1, 1, 1, .6, 25, 25, 0, 0, head)
  elma.renderText(1, 1, 1, .8, 25, 29, 0, 1, slot0)
  let status = slots.slot.status
  if (status == "DEAD") {
    elma.renderText(1, .2, .2, .8, 25, 29, slot0.length, 1, status)
  } else {
    elma.renderText(1, 1, 1, .8, 25, 29, slot0.length, 1, status)
  }

  if (paused) {
    let events = slots.slot.getKeyEvents(8)

    elma.renderBox(0, 0, 0, a, -10, 80, 10, 12, 15, 15)
    elma.renderText(1, 1, 1, 1, -25, 95, 0, 0, "PAUSED      ")

    if (events.length) {
      elma.renderBox(0, 1, 1, a, -10, 130 + frameEdit * 20, 1, 12, 15, 6)
    }

    events.forEach(([f, k], i) => {
      elma.renderText(1, 1, 1, 1, -25, 95, 0, 2+i, `${String(f).padStart(6, ' ')}  ${renderKeys(k)}`)
    })
  }
}

function renderKeys(k: number) {
  return `${k&1?'G':'_'}${k&2?'B':'_'}${k&4?'L':'_'}${k&8?'R':'_'}${k&16?'T':'_'}`
}


function timing() {
  let [sec, ns] = process.hrtime()
  return sec*1000 + ns / (10**6)
}


// SDL ScanCode
const SSC: { [k: string]: number } = {
    RETURN: 40,
    SEMICOLON: 51,
    APOSTROPHE: 52,
    MINUS: 45,
    EQUALS: 46,
    LEFTBRACKET: 47,
    RIGHTBRACKET: 48,
    ESCAPE: 41,
    BACKSPACE: 42,
    TAB: 43,
    SPACE: 44,
    _1: 30,
    _2: 31,
    _3: 32,
    _4: 33,
    _5: 34,
    _6: 35,
    _7: 36,
    _8: 37,
    _9: 38,
    _0: 39,
    COMMA: 54,
    PERIOD: 55,
    SLASH: 56,
    LCTRL: 224,
    LSHIFT: 225,
    LALT: 226,
    LGUI: 227,
    RCTRL: 228,
    RSHIFT: 229,
    A: 4,
    B: 5,
    C: 6,
    D: 7,
    E: 8,
    F: 9,
    G: 10,
    H: 11,
    I: 12,
    J: 13,
    K: 14,
    L: 15,
    M: 16,
    N: 17,
    O: 18,
    P: 19,
    Q: 20,
    R: 21,
    S: 22,
    T: 23,
    U: 24,
    V: 25,
    W: 26,
    X: 27,
    Y: 28,
    Z: 29,
    HOME: 74,
    PAGEUP: 75,
    DELETE: 76,
    END: 77,
    PAGEDOWN: 78,
    RIGHT: 79,
    LEFT: 80,
    DOWN: 81,
    UP: 82,
}


const KEY_TURN = SSC[process.env['ELMA_KEY_TURN'] || 'SPACE']

main()
