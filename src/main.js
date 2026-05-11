// @ts-ignore
import elma from 'elma.node';
import path from 'path';
import * as process from 'process';
import { glob } from 'glob';
const FPS = 300;
const levPat = "LukL*.lev";
async function main() {
    process.chdir(process.env.ELMA_DIR);
    let levs = await glob(`./lev/${levPat}`);
    if (levs.length == 0) {
        console.warn("No levels matched");
        process.exit(1);
    }
    levs = levs.map((l) => path.basename(l));
    elma.elmaInit();
    //elma.setQuality(2);
    elma.setDT(0.182 * .0024 * (1000 / FPS));
    let lev = levs[0];
    //  let l = 0
    //
    //  while (true) {
    //    let lev = levs[l%levs.length]
    //    let r = await gameloop(lev!)
    //    if (r == 'next') {
    //      l++
    //    } else if (r == 'prev') {
    //      l = levs.length + l - 1
    //    } else {
    //      break
    //    }
    //  }
    //}
    //
    //async function gameloop(lev: string) {
    console.log("lev", lev);
    let slots = new Slots(elma.initGame(lev));
    let paused = false;
    let _speed = 1;
    let lastRender = timing();
    for (let l = 0;; l++) {
        let speed = _speed;
        elma.handleEvents();
        let done = slots.sim.dead() || slots.sim.finished();
        const lctrl = elma.keyIsDown(SDL_SCANCODE_LCTRL);
        const lshift = elma.keyIsDown(SDL_SCANCODE_LSHIFT);
        if (lctrl && elma.keyJustPressed(SDL_SCANCODE_N)) {
            return "next";
        }
        else if (lctrl && elma.keyJustPressed(SDL_SCANCODE_P)) {
            return "prev";
        }
        if (elma.keyJustPressed(SDL_SCANCODE_S)) {
            slots.save();
        }
        else if (elma.keyJustPressed(SDL_SCANCODE_L)) {
            paused = true;
            slots.load();
            elma.render(slots.sim);
        }
        if (elma.keyJustPressed(SDL_SCANCODE_UP) || elma.keyJustPressed(SDL_SCANCODE_DOWN)) {
            slots.slot.truncate();
            paused = false;
        }
        if (lctrl && elma.keyJustPressed(SDL_SCANCODE_Q))
            break;
        if (lshift && elma.keyJustPressed(SDL_SCANCODE_ESCAPE))
            break;
        if (!lctrl && elma.keyJustPressed(SDL_SCANCODE_RETURN))
            paused = !paused;
        if (paused && elma.keyJustPressed(SDL_SCANCODE_N)) {
            slots.slot.step();
            slots.save();
            elma.render(slots.slot.sim);
        }
        if (!lctrl && paused && elma.keyJustPressed(SDL_SCANCODE_B)) {
            slots.slot.back();
            slots.save();
            elma.render(slots.slot.sim);
        }
        if (!lctrl && elma.keyJustPressed(SDL_SCANCODE_R)) {
            paused = true;
            slots.slot.reset();
            elma.render(slots.slot.sim);
        }
        if (!lctrl && elma.keyJustPressed(SDL_SCANCODE_BACKSPACE))
            slots.slot.truncate();
        if (elma.keyJustPressed(SDL_SCANCODE_APOSTROPHE)) {
            speed = _speed = lshift ? .6 : .3;
        }
        else if (elma.keyJustPressed(SDL_SCANCODE_SEMICOLON)) {
            speed = _speed = lshift ? .075 : .15;
        }
        else if (elma.keyJustPressed(SDL_SCANCODE_SLASH)) {
            speed = _speed = 1;
        }
        else {
            let lbrac = elma.keyIsDown(SDL_SCANCODE_LEFTBRACKET);
            let rbrac = elma.keyIsDown(SDL_SCANCODE_RIGHTBRACKET);
            if (lbrac || (rbrac && !done)) {
                paused = true;
                //slot.rewindcache ||= {}
                speed = 1;
                let n = lctrl ? 1 : lshift ? 12 : 4;
                lbrac ? slots.slot.back(n) : slots.slot.step(n);
                if (l % (lshift ? 1 : 1) == 0)
                    elma.render(slots.slot.sim);
                if (lctrl)
                    speed = .3;
            }
            else {
                slots.slot.rewindcache = null;
            }
        }
        // Elma classic doesnt seem to support live zoom :(
        //
        //if (elma.keyJustPressed(SDL_SCANCODE_MINUS)) {
        //  elma.adjustZoom(-.01)
        //}
        //if (elma.keyJustPressed(SDL_SCANCODE_EQUALS)) {
        //  elma.adjustZoom(.01)
        //}
        let render = false;
        if (!paused && !done) {
            slots.slot.step();
            render = true;
            if (slots.slot.sim.dead() || slots.slot.sim.finished()) {
                paused = true;
            }
        }
        let ms = (1000 / FPS) / speed;
        let now = timing();
        let delay = (lastRender + ms) - now;
        if (delay > 0)
            elma.sleep(delay);
        if (render)
            elma.render(slots.slot.sim);
        if (delay > 0) {
            lastRender += ms;
        }
        else {
            lastRender = now;
            //console.warn("frame lag")
        }
    }
}
class Slots {
    constructor(sim) {
        this.slots = [new Slot(sim)];
        this.current = 0;
        this.saves = [];
    }
    save() {
        this.saves[this.current] = this.slots[this.current].copy();
    }
    load() {
        if (this.saves[this.current]) {
            this.slots[this.current] = this.saves[this.current].copy();
        }
    }
    get slot() {
        return this.slots[this.current];
    }
    get sim() {
        return this.slot.sim;
    }
}
const CHECKPOINT_INTERVAL = 50;
class Slot {
    constructor(sim) {
        this.sim = sim;
        this.rewindcache = null;
        this.sim = sim;
        this.frame = 0;
        this.keys = [];
        this.checkpoints = [];
    }
    copy() {
        let slot = new Slot(this.sim.copy());
        slot.frame = this.frame;
        slot.keys = this.keys.slice();
        slot.checkpoints = this.checkpoints.slice();
        return slot;
    }
    step(n = 1) {
        for (let i = 0; i < n; i++) {
            let keys = 0;
            if (this.frame < this.keys.length) {
                keys = this.keys[this.frame];
            }
            else {
                if (this.frame % CHECKPOINT_INTERVAL == 0) {
                    this.checkpoints[this.frame / CHECKPOINT_INTERVAL] = this.sim.copy();
                }
                keys = ((elma.keyIsDown(SDL_SCANCODE_UP) ? 1 : 0) +
                    (elma.keyIsDown(SDL_SCANCODE_DOWN) ? 2 : 0) +
                    (elma.keyIsDown(SDL_SCANCODE_LEFT) ? 4 : 0) +
                    (elma.keyIsDown(SDL_SCANCODE_RIGHT) ? 8 : 0) +
                    (elma.keyJustPressed(SDL_SCANCODE_LCTRL) ? 16 : 0));
                this.keys[this.frame] = keys;
            }
            this.sim.step(keys);
            this.frame++;
        }
    }
    back(n = 1) {
        for (let j = 0; j < n; j++) {
            if (this.frame == 0)
                return;
            this.frame--;
            if (this.rewindcache && this.rewindcache[this.frame]) {
                this.sim = this.rewindcache[this.frame].copy();
            }
            else {
                let checkidx = Math.floor(this.frame / CHECKPOINT_INTERVAL);
                let off = this.frame % CHECKPOINT_INTERVAL;
                if (this.rewindcache && off == CHECKPOINT_INTERVAL - 1) {
                    this.rewindcache = {};
                }
                let check = this.checkpoints[checkidx];
                this.sim = check.copy();
                for (let i = 0; i < off; i++) {
                    let f = checkidx * CHECKPOINT_INTERVAL + i;
                    let keys = this.keys[f];
                    this.sim.step(keys);
                    if (this.rewindcache) {
                        this.rewindcache[f] = this.sim.copy();
                    }
                }
            }
        }
    }
    reset() {
        this.sim = this.checkpoints[0];
        this.checkpoints.length = 1;
        this.keys = [];
        this.frame = 0;
    }
    truncate() {
        this.keys.length = this.frame;
    }
}
function timing() {
    let [sec, ns] = process.hrtime();
    return sec * 1000 + ns / (10 ** 6);
}
const SDL_SCANCODE_RETURN = 40;
const SDL_SCANCODE_SEMICOLON = 51;
const SDL_SCANCODE_APOSTROPHE = 52;
const SDL_SCANCODE_MINUS = 45;
const SDL_SCANCODE_EQUALS = 46;
const SDL_SCANCODE_LEFTBRACKET = 47;
const SDL_SCANCODE_RIGHTBRACKET = 48;
const SDL_SCANCODE_ESCAPE = 41;
const SDL_SCANCODE_BACKSPACE = 42;
const SDL_SCANCODE_TAB = 43;
const SDL_SCANCODE_SPACE = 44;
const SDL_SCANCODE_1 = 30;
const SDL_SCANCODE_2 = 31;
const SDL_SCANCODE_3 = 32;
const SDL_SCANCODE_4 = 33;
const SDL_SCANCODE_5 = 34;
const SDL_SCANCODE_6 = 35;
const SDL_SCANCODE_7 = 36;
const SDL_SCANCODE_8 = 37;
const SDL_SCANCODE_9 = 38;
const SDL_SCANCODE_0 = 39;
const SDL_SCANCODE_COMMA = 54;
const SDL_SCANCODE_PERIOD = 55;
const SDL_SCANCODE_SLASH = 56;
const SDL_SCANCODE_LCTRL = 224;
const SDL_SCANCODE_LSHIFT = 225;
const SDL_SCANCODE_LALT = 226;
const SDL_SCANCODE_LGUI = 227;
const SDL_SCANCODE_RCTRL = 228;
const SDL_SCANCODE_RSHIFT = 229;
const SDL_SCANCODE_A = 4;
const SDL_SCANCODE_B = 5;
const SDL_SCANCODE_C = 6;
const SDL_SCANCODE_D = 7;
const SDL_SCANCODE_E = 8;
const SDL_SCANCODE_F = 9;
const SDL_SCANCODE_G = 10;
const SDL_SCANCODE_H = 11;
const SDL_SCANCODE_I = 12;
const SDL_SCANCODE_J = 13;
const SDL_SCANCODE_K = 14;
const SDL_SCANCODE_L = 15;
const SDL_SCANCODE_M = 16;
const SDL_SCANCODE_N = 17;
const SDL_SCANCODE_O = 18;
const SDL_SCANCODE_P = 19;
const SDL_SCANCODE_Q = 20;
const SDL_SCANCODE_R = 21;
const SDL_SCANCODE_S = 22;
const SDL_SCANCODE_T = 23;
const SDL_SCANCODE_U = 24;
const SDL_SCANCODE_V = 25;
const SDL_SCANCODE_W = 26;
const SDL_SCANCODE_X = 27;
const SDL_SCANCODE_Y = 28;
const SDL_SCANCODE_Z = 29;
const SDL_SCANCODE_HOME = 74;
const SDL_SCANCODE_PAGEUP = 75;
const SDL_SCANCODE_DELETE = 76;
const SDL_SCANCODE_END = 77;
const SDL_SCANCODE_PAGEDOWN = 78;
const SDL_SCANCODE_RIGHT = 79;
const SDL_SCANCODE_LEFT = 80;
const SDL_SCANCODE_DOWN = 81;
const SDL_SCANCODE_UP = 82;
main();
//# sourceMappingURL=main.js.map