"""Alien Sim — Player Controlled
WASD / Arrow keys to steer. No click needed — keyboard is captured at the
document level and pushed into Bokeh via its own model-sync protocol.
"""
import json
import math
import pathlib

import panel as pn
from bokeh.plotting import figure
from bokeh.models import ColumnDataSource, Range1d

from model import Model, CrewState

pn.extension("bokeh")

# ─── Config ───────────────────────────────────────────────────────────────────
configuration_file = pathlib.Path(__file__).parents[1] / "data" / "config.json"
config = json.load(open(configuration_file))
WORLD  = config.get("region_limit", 100)

STATE_COLOR = {
    CrewState.IDLE:     "#B8B8B8",
    CrewState.ALERTED:  "#FF8C00",
    CrewState.FLEEING:  "#FFD700",
    CrewState.HIDING:   "#228B22",
    CrewState.ESCAPING: "#00CED1",
}

# ─── Keyboard state ────────────────────────────────────────────────────────────
# Two parallel mechanisms so at least one fires:
#
#  A) JS writes to a named ColumnDataSource ("kbd") via Bokeh's model-sync
#     protocol.  run_model() reads it directly — no Python callback needed.
#
#  B) Bokeh on_event(KeyDown/KeyUp) fires when the canvas has focus (user
#     clicked the plot).  Updates _dir which run_model() also reads.
#
# Whichever delivers a non-zero vector first wins each frame.

_dir     = [0.0, 0.0]     # set by mechanism B
_pressed = set()
TRACKED  = {'w','a','s','d','ArrowUp','ArrowDown','ArrowLeft','ArrowRight'}

def _update_dir():
    dx, dy = 0.0, 0.0
    if 'a' in _pressed or 'ArrowLeft'  in _pressed: dx -= 1.0
    if 'd' in _pressed or 'ArrowRight' in _pressed: dx += 1.0
    if 'w' in _pressed or 'ArrowUp'    in _pressed: dy += 1.0
    if 's' in _pressed or 'ArrowDown'  in _pressed: dy -= 1.0
    _dir[0], _dir[1] = dx, dy

def _key_down(event):
    if event.key in TRACKED:
        _pressed.add(event.key); _update_dir()

def _key_up(event):
    _pressed.discard(event.key); _update_dir()

# ─── Data sources ─────────────────────────────────────────────────────────────
# Mechanism A: keyboard state arrives here via Bokeh model-sync from the browser
kbd_src = ColumnDataSource(data=dict(dx=[0.0], dy=[0.0]), name="kbd")

# Ship geometry
corr_src       = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))
vent_shaft_src = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))
 
bridge_src     = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))
medbay_src     = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))
comms_src      = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))
armory_src     = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))
crew_src_room  = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))  # crew quarters rooms
storage_src    = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))
reactor_src    = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))
engine_src     = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))  # EngineRoom + EnginePod
vent_src       = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))  # VentShaft rooms
shuttle_src    = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))
norm_src       = ColumnDataSource(dict(left=[], right=[], top=[], bottom=[]))  # Normal / fallback
 
hull_src       = ColumnDataSource(dict(xs=[[]], ys=[[]]))

# Simulation layers — named so JS overlay can find them
trail_src = ColumnDataSource(dict(x=[], y=[]),             name="trail")
crew_src  = ColumnDataSource(dict(x=[], y=[], c=[], s=[]), name="crew")
pings_src = ColumnDataSource(dict(x=[], y=[], s=[]),       name="pings")
pred_src  = ColumnDataSource(dict(x=[], y=[], c=[], s=[]), name="pred")

# ─── Figure ───────────────────────────────────────────────────────────────────
p = figure(
    width=620, height=620,
    x_range=Range1d(-WORLD, WORLD),
    y_range=Range1d(-WORLD, WORLD),
    background_fill_color="#060D14",
    border_fill_color="#060D14",
    toolbar_location=None,
    output_backend="webgl",
)
p.xaxis.visible = p.yaxis.visible = p.grid.visible = False

# Hull outline — drawn first so it sits behind everything
p.patch('xs', 'ys', source=hull_src,
        color="#0A1520", line_color="#1A3A5C", line_width=1.5, alpha=0.9)
 
# Corridors
p.quad(source=corr_src,       left='left', right='right', top='top', bottom='bottom',
       color="#0D1822", line_color=None)
p.quad(source=vent_shaft_src, left='left', right='right', top='top', bottom='bottom',
       color="#0A120A", line_color="#1A2A1A", line_width=0.5, line_dash="dashed")
 
# Rooms — each type gets its own colour
# Normal / fallback
p.quad(source=norm_src,      left='left', right='right', top='top', bottom='bottom',
       color="#1C2B3A", line_color="#2A4060", line_width=1)
# Bridge — bright blue accent
p.quad(source=bridge_src,    left='left', right='right', top='top', bottom='bottom',
       color="#0D2040", line_color="#2060C0", line_width=1.5)
# Medbay — clinical teal
p.quad(source=medbay_src,    left='left', right='right', top='top', bottom='bottom',
       color="#0D2828", line_color="#1A6060", line_width=1)
# Comms — purple hint
p.quad(source=comms_src,     left='left', right='right', top='top', bottom='bottom',
       color="#1A1030", line_color="#3A2060", line_width=1)
# Armory — dark red
p.quad(source=armory_src,    left='left', right='right', top='top', bottom='bottom',
       color="#200D0D", line_color="#602020", line_width=1)
# Crew quarters — warm grey
p.quad(source=crew_src_room, left='left', right='right', top='top', bottom='bottom',
       color="#1C2020", line_color="#384040", line_width=1)
# Storage — muted brown
p.quad(source=storage_src,   left='left', right='right', top='top', bottom='bottom',
       color="#1C1810", line_color="#403020", line_width=1)
# Reactor — amber glow
p.quad(source=reactor_src,   left='left', right='right', top='top', bottom='bottom',
       color="#201400", line_color="#604000", line_width=1.5)
# Engine — orange
p.quad(source=engine_src,    left='left', right='right', top='top', bottom='bottom',
       color="#201000", line_color="#804010", line_width=1.5)
# Vent shaft rooms — dark green
p.quad(source=vent_src,      left='left', right='right', top='top', bottom='bottom',
       color="#0D1F0D", line_color="#1A3520", line_width=1)
# Shuttle bay — cold blue
p.quad(source=shuttle_src,   left='left', right='right', top='top', bottom='bottom',
       color="#0D1A2E", line_color="#1A3860", line_width=1.5)

# Sim layers — kept at alpha=0; JS canvas overlay draws sprites instead
p.scatter('x','y', source=trail_src, color="#556677", size=2,  alpha=0,    line_color=None)
p.scatter('x','y', source=crew_src,  color='c',       size='s',alpha=0,    line_color=None)
p.scatter('x','y', source=pings_src, color="#FF6600", size='s',alpha=0,    line_color=None)
p.scatter('x','y', source=pred_src,  color='c',       size='s',alpha=0,    line_color=None)

# Anchor kbd_src to the figure (alpha=0 → invisible) so Bokeh includes it in
# the client-side document and JS can find it by name.
p.scatter('dx','dy', source=kbd_src, size=1, alpha=0, line_color=None)


plot_pane = pn.pane.Bokeh(p, sizing_mode="fixed")

# ─── JS keyboard bridge (mechanism A) ────────────────────────────────────────
# Listens on the document (no focus needed), finds the named ColumnDataSource
# via Bokeh's own JS model API, and writes dx/dy into it.  Bokeh's sync
# protocol propagates the change to the Python server automatically.
keyboard_js = pn.pane.HTML("""
<script>
(function () {
    "use strict";

    // ── Keyboard bridge (unchanged) ───────────────────────────────────────────
    const pressed = {};
    const TRACKED = new Set(
        ['w','a','s','d','ArrowUp','ArrowDown','ArrowLeft','ArrowRight']);

    let kbdSrc = null;
    function findSrc() {
        try {
            const doc = Bokeh && Bokeh.documents && Bokeh.documents[0];
            if (doc) kbdSrc = doc.get_model_by_name('kbd');
        } catch (_) {}
        if (!kbdSrc) setTimeout(findSrc, 250);
    }
    findSrc();

    function push() {
        if (!kbdSrc) { findSrc(); return; }
        let dx = 0, dy = 0;
        if (pressed['a'] || pressed['ArrowLeft'])  dx -= 1;
        if (pressed['d'] || pressed['ArrowRight']) dx += 1;
        if (pressed['w'] || pressed['ArrowUp'])    dy += 1;
        if (pressed['s'] || pressed['ArrowDown'])  dy -= 1;
        kbdSrc.data = {dx: [dx], dy: [dy]};
    }

    document.addEventListener('keydown', e => {
        if (!TRACKED.has(e.key)) return;
        e.preventDefault(); pressed[e.key] = true; push();
    });
    document.addEventListener('keyup', e => {
        if (!TRACKED.has(e.key)) return;
        delete pressed[e.key]; push();
    });
    window.addEventListener('blur', () => {
        for (const k of Object.keys(pressed)) delete pressed[k]; push();
    });

    function focusCanvas() {
        const c = document.querySelector('canvas.bk-canvas');
        if (!c) { setTimeout(focusCanvas, 300); return; }
        c.setAttribute('tabindex', '1');
        c.focus();
    }
    setTimeout(focusCanvas, 500);

    // ── Sprite + Particle overlay ─────────────────────────────────────────────
    //
    // Strategy:
    //   1. Wait for the Bokeh canvas to exist, then create a Canvas2D overlay
    //      positioned exactly on top of it.
    //   2. Each rAF, read the named ColumnDataSources (crew, pred, trail, pings)
    //      that Python updates every tick, convert world→pixel coords using
    //      Bokeh's own x_range/y_range, and draw sprites.
    //   3. Particles are pure JS — spawned on crew-catch events and on pings,
    //      never touching the Python server.
    //
    // Sprite files expected at dashboard/static/:
    //   xeno.png        — xenomorph, any size, will be drawn 48×48 px (or scaled)
    //   xeno_locked.png — xenomorph with yellow glow (locked-on state)
    //   xeno_egg.png    — pulsing egg (hatching state)
    //   crew_idle.png   — crew dot, ~16×16 px
    //   crew_alert.png  — crew alerted
    //   crew_flee.png   — crew fleeing
    //   crew_hide.png   — crew hiding
    //   crew_escape.png — crew escaping
    //
    // All sprites are centred on the agent position.
    // If a file fails to load, a coloured circle fallback is drawn instead.

    const SPRITE_DIR = '/sprites/';

    // Load a sprite, return an Image object (may not be complete yet)
    function loadSprite(name) {
        const img = new Image();
        img.src = SPRITE_DIR + name;
        img.onerror = () => { img._failed = true; };
        return img;
    }

    const SPRITES = {
        xeno:        loadSprite('xeno.png'),
        xeno_locked: loadSprite('xeno_locked.png'),
        xeno_egg:    loadSprite('xeno_egg.png'),
        crew_idle:   loadSprite('crew_idle.png'),
        crew_alert:  loadSprite('crew_alert.png'),
        crew_flee:   loadSprite('crew_flee.png'),
        crew_hide:   loadSprite('crew_hide.png'),
        crew_escape: loadSprite('crew_escape.png'),
    };

    // Fallback colours when sprite hasn't loaded
    const FALLBACK = {
        xeno: '#FF2244', xeno_locked: '#FFE020', xeno_egg: '#FFFFFF',
        crew_idle: '#B8B8B8', crew_alert: '#FF8C00', crew_flee: '#FFD700',
        crew_hide: '#228B22', crew_escape: '#00CED1',
    };

    // Crew state index → sprite key  (matches Python CrewState enum order)
    // CrewState values: IDLE=0 ALERTED=1 FLEEING=2 HIDING=3 ESCAPING=4
    const CREW_SPRITE = ['crew_idle','crew_alert','crew_flee','crew_hide','crew_escape'];
    // Colour strings for the 'c' field Python writes
    const STATE_COLOR_TO_IDX = {
        '#B8B8B8': 0,  // IDLE
        '#FF8C00': 1,  // ALERTED
        '#FFD700': 2,  // FLEEING
        '#228B22': 3,  // HIDING
        '#00CED1': 4,  // ESCAPING
    };

    // ── Particle system ───────────────────────────────────────────────────────
    // Each particle: {x, y, vx, vy, life, maxLife, r, g, b, size}
    const particles = [];

    function spawnAcidSplatter(px, py) {
        // Green acid blood — burst of 18 particles
        for (let i = 0; i < 18; i++) {
            const angle = Math.random() * Math.PI * 2;
            const speed = 0.4 + Math.random() * 1.8;
            particles.push({
                x: px, y: py,
                vx: Math.cos(angle) * speed,
                vy: Math.sin(angle) * speed,
                life: 1.0, maxLife: 1.0,
                r: 20 + Math.floor(Math.random()*60),
                g: 180 + Math.floor(Math.random()*75),
                b: 20,
                size: 2 + Math.random() * 3,
            });
        }
    }

    function spawnPingRipple(px, py) {
        // Orange alert ring — 12 outward sparks
        for (let i = 0; i < 12; i++) {
            const angle = (i / 12) * Math.PI * 2;
            const speed = 0.6 + Math.random() * 0.8;
            particles.push({
                x: px, y: py,
                vx: Math.cos(angle) * speed,
                vy: Math.sin(angle) * speed,
                life: 1.0, maxLife: 0.7,
                r: 255, g: 120 + Math.floor(Math.random()*80), b: 0,
                size: 1.5 + Math.random() * 2,
            });
        }
    }

    function spawnScreenFlash() {
        // Brief white overlay on the canvas — handled separately via _flashAlpha
        _flashAlpha = 0.45;
    }

    let _flashAlpha = 0;

    function updateParticles(dt) {
        for (let i = particles.length - 1; i >= 0; i--) {
            const p = particles[i];
            p.x    += p.vx * dt * 60;
            p.y    += p.vy * dt * 60;
            p.vx   *= 0.93;
            p.vy   *= 0.93;
            p.life -= dt / p.maxLife;
            if (p.life <= 0) particles.splice(i, 1);
        }
        if (_flashAlpha > 0) _flashAlpha = Math.max(0, _flashAlpha - dt * 3.5);
    }

    // ── Overlay canvas ────────────────────────────────────────────────────────
    let overlay = null;
    let ctx     = null;

    // Bokeh model references — resolved once after load
    let crewSrc  = null;
    let predSrc  = null;
    let pingSrc  = null;
    let bokehPlot = null;  // the Plot model (has x_range / y_range)

    let _prevCrewCount = -1;
    let _prevPingCount = 0;

    function findModels() {
        try {
            const doc = Bokeh.documents[0];
            crewSrc   = doc.get_model_by_name('crew');
            predSrc   = doc.get_model_by_name('pred');
            pingSrc   = doc.get_model_by_name('pings');
            // The Plot model is the first Plot in the document
            bokehPlot = doc.get_model_by_type('Plot');
        } catch(_) {}
        if (!crewSrc || !predSrc || !bokehPlot) {
            setTimeout(findModels, 300);
            return;
        }
        initOverlay();
    }

    function initOverlay() {
        // Find the Bokeh canvas container and size our overlay to match exactly
        function tryInit() {
            const bkCanvas = document.querySelector('canvas.bk-canvas');
            if (!bkCanvas) { setTimeout(tryInit, 300); return; }

            overlay = document.createElement('canvas');
            overlay.style.position = 'absolute';
            overlay.style.pointerEvents = 'none';  // clicks pass through to Bokeh
            overlay.style.imageRendering = 'pixelated';
            overlay.style.left = '0px';
            overlay.style.top  = '0px';
            bkCanvas.parentElement.style.position = 'relative';
            bkCanvas.parentElement.appendChild(overlay);

            function syncSize() {
                overlay.width  = bkCanvas.width;
                overlay.height = bkCanvas.height;
                overlay.style.width  = bkCanvas.style.width  || bkCanvas.width  + 'px';
                overlay.style.height = bkCanvas.style.height || bkCanvas.height + 'px';
            }
            syncSize();
            new ResizeObserver(syncSize).observe(bkCanvas);

            ctx = overlay.getContext('2d');
            bkCanvas.setAttribute('tabindex', '1');
            bkCanvas.focus();

            requestAnimationFrame(renderLoop);
        }
        tryInit();
    }

    // ── Coordinate transform ──────────────────────────────────────────────────
    // Bokeh world coords → overlay pixel coords.
    // Bokeh's y-axis is mathematical (up=positive); canvas y is inverted.
    function worldToPixel(wx, wy) {
        if (!bokehPlot || !overlay) return [0, 0];
        const xr = bokehPlot.x_range;
        const yr = bokehPlot.y_range;
        // Account for Bokeh's plot frame inset (axes area).
        // We probe the bk-canvas bounding vs the plot frame.
        const frame = bokehPlot.frame;
        const fw = frame ? frame._width  : overlay.width;
        const fh = frame ? frame._height : overlay.height;
        const fl = frame ? frame._left   : 0;
        const ft = frame ? frame._top    : 0;

        const px = fl + (wx - xr.start) / (xr.end - xr.start) * fw;
        const py = ft + (1 - (wy - yr.start) / (yr.end - yr.start)) * fh;
        return [px, py];
    }

    // ── Draw a sprite centred at (px,py), size w×h pixels ────────────────────
    // Falls back to a filled circle if the image hasn't loaded.
    function drawSprite(key, px, py, w, h, alpha, angle) {
        const img = SPRITES[key];
        ctx.save();
        ctx.globalAlpha = alpha;
        ctx.translate(px, py);
        if (angle) ctx.rotate(angle);
        if (img && img.complete && !img._failed) {
            ctx.drawImage(img, -w/2, -h/2, w, h);
        } else {
            // Fallback circle
            ctx.beginPath();
            ctx.arc(0, 0, w/2, 0, Math.PI*2);
            ctx.fillStyle = FALLBACK[key] || '#888';
            ctx.fill();
        }
        ctx.restore();
    }

    // ── Main render loop ──────────────────────────────────────────────────────
    let _lastT = null;
    function renderLoop(t) {
        requestAnimationFrame(renderLoop);
        if (!ctx || !crewSrc || !predSrc) return;

        const dt = _lastT ? Math.min((t - _lastT) / 1000, 0.05) : 0.016;
        _lastT = t;

        // ── Event detection ───────────────────────────────────────────────────
        const crewCount = crewSrc.data.x ? crewSrc.data.x.length : 0;
        const pingCount = pingSrc && pingSrc.data.x ? pingSrc.data.x.length : 0;

        // Crew catch — someone disappeared this frame
        if (_prevCrewCount > 0 && crewCount < _prevCrewCount) {
            const px = predSrc.data.x[0];
            const py = predSrc.data.y[0];
            if (px !== undefined) {
                const [spx, spy] = worldToPixel(px, py);
                spawnAcidSplatter(spx, spy);
                spawnScreenFlash();
            }
        }
        _prevCrewCount = crewCount;

        // New pings — spawn ripple particles
        if (pingCount > _prevPingCount && pingSrc.data.x) {
            for (let i = _prevPingCount; i < pingCount; i++) {
                const [spx, spy] = worldToPixel(pingSrc.data.x[i], pingSrc.data.y[i]);
                spawnPingRipple(spx, spy);
            }
        }
        _prevPingCount = pingCount;

        updateParticles(dt);

        // ── Clear ─────────────────────────────────────────────────────────────
        ctx.clearRect(0, 0, overlay.width, overlay.height);

        // ── Acid splatter / ping particles ────────────────────────────────────
        for (const p of particles) {
            ctx.save();
            ctx.globalAlpha = Math.max(0, p.life) * 0.9;
            ctx.beginPath();
            ctx.arc(p.x, p.y, p.size, 0, Math.PI*2);
            ctx.fillStyle = `rgb(${p.r},${p.g},${p.b})`;
            ctx.fill();
            ctx.restore();
        }

        // ── Crew sprites ──────────────────────────────────────────────────────
        if (crewSrc.data.x) {
            const xs = crewSrc.data.x;
            const ys = crewSrc.data.y;
            const cs = crewSrc.data.c;
            for (let i = 0; i < xs.length; i++) {
                const [px, py] = worldToPixel(xs[i], ys[i]);
                const stateIdx = STATE_COLOR_TO_IDX[cs[i]] ?? 0;
                const spriteKey = CREW_SPRITE[stateIdx];
                drawSprite(spriteKey, px, py, 14, 14, 0.88, 0);
            }
        }

        // ── Predator sprite ───────────────────────────────────────────────────
        if (predSrc.data.x && predSrc.data.x.length > 0) {
            const wx = predSrc.data.x[0];
            const wy = predSrc.data.y[0];
            const col = predSrc.data.c[0];
            const sz  = predSrc.data.s[0] || 10;
            const [px, py] = worldToPixel(wx, wy);

            // Derive state from colour Python writes
            let spriteKey, spriteSize;
            if (col === '#FFFFFF') {
                // Hatching — pulse scale
                const pulse = 0.85 + 0.15 * Math.sin(Date.now() / 160);
                spriteKey  = 'xeno_egg';
                spriteSize = (sz / 10) * 52 * pulse;
            } else if (col === '#FFE020') {
                spriteKey  = 'xeno_locked';
                spriteSize = (sz / 10) * 52;
            } else {
                spriteKey  = 'xeno';
                spriteSize = (sz / 10) * 52;
            }

            // Glow ring for locked-on state
            if (col === '#FFE020') {
                ctx.save();
                ctx.beginPath();
                ctx.arc(px, py, spriteSize * 0.7, 0, Math.PI * 2);
                ctx.strokeStyle = 'rgba(255,224,32,0.25)';
                ctx.lineWidth   = 6;
                ctx.stroke();
                ctx.restore();
            }

            drawSprite(spriteKey, px, py, spriteSize, spriteSize, 1.0, 0);
        }

        // ── Screen flash on kill ───────────────────────────────────────────────
        if (_flashAlpha > 0) {
            ctx.save();
            ctx.globalAlpha = _flashAlpha;
            ctx.fillStyle   = '#FFFFFF';
            ctx.fillRect(0, 0, overlay.width, overlay.height);
            ctx.restore();
        }
    }

    // Kick off model search after Bokeh finishes loading
    setTimeout(findModels, 800);

})();
</script>
""", height=0, width=0, margin=0)


# Room-type → (source, 'key') mapping.
# NOTE: Python bindings expose RoomType as an int or enum value.
#       Adjust the comparison below to match how your pybind11 binding exposes it.
#       If RoomType is exposed as a Python IntEnum called RoomType, use:
#           from your_module import RoomType
#       and compare with RoomType.Bridge, etc.
#       If it's exposed as a plain int, compare with the numeric values:
#           0=Normal, 1=Bridge, 2=Medbay, 3=Comms, 4=Armory, 5=CrewQuarters,
#           6=Storage, 7=Reactor, 8=EngineRoom, 9=EnginePod, 10=ShuttleBay, 11=VentShaft
 
def _room_bucket(r):
    """Return the ColumnDataSource for a given room's type."""
    # FIX: removed broken 'from your_module import RoomType' placeholder.
    # Use the boolean properties now fully exposed by the pybind11 binding.
    if r.is_bridge:        return bridge_src
    if r.is_medbay:        return medbay_src
    if r.is_comms:         return comms_src
    if r.is_armory:        return armory_src
    if r.is_crew_qrtrs:    return crew_src_room
    if r.is_storage:       return storage_src
    if r.is_reactor:       return reactor_src
    if r.is_engine:        return engine_src
    if r.is_vent:          return vent_src
    if r.is_shuttle_bay:   return shuttle_src
    return norm_src
 
 
def _update_ship_sources(ship):
    # Corridors
    cl, cr, ct, cb   = [], [], [], []   # main corridors
    vl, vr, vt, vb   = [], [], [], []   # vent shafts
 
    for c in ship.corridors:
        for ax, ay, bx, by in [(c.x1, c.y1, c.bx, c.by),
                                (c.bx, c.by, c.x2, c.y2)]:
            l = min(ax, bx) - c.hw;  r = max(ax, bx) + c.hw
            b = min(ay, by) - c.hw;  t = max(ay, by) + c.hw
            if c.is_vent_shaft:
                vl.append(l); vr.append(r); vt.append(t); vb.append(b)
            else:
                cl.append(l); cr.append(r); ct.append(t); cb.append(b)
 
    corr_src.data       = dict(left=cl, right=cr, top=ct, bottom=cb)
    vent_shaft_src.data = dict(left=vl, right=vr, top=vt, bottom=vb)
 
    # Hull polygon
    if ship.hull_xs and ship.hull_ys:
        hull_src.data = dict(xs=[list(ship.hull_xs)], ys=[list(ship.hull_ys)])
    else:
        hull_src.data = dict(xs=[[]], ys=[[]])
 
    # Rooms — clear all buckets first
    all_room_srcs = [
        bridge_src, medbay_src, comms_src, armory_src, crew_src_room,
        storage_src, reactor_src, engine_src, vent_src, shuttle_src, norm_src,
    ]
    buckets = {src: ([], [], [], []) for src in all_room_srcs}  # left,right,top,bottom
 
    for r in ship.rooms:
        src = _room_bucket(r)
        ll, rr, tt, bb = buckets[src]
        ll.append(r.cx - r.hw)
        rr.append(r.cx + r.hw)
        tt.append(r.cy + r.hh)
        bb.append(r.cy - r.hh)
 
    for src, (ll, rr, tt, bb) in buckets.items():
        src.data = dict(left=ll, right=rr, top=tt, bottom=bb)


# ─── Model ────────────────────────────────────────────────────────────────────
model_state       = Model(str(configuration_file))
periodic_callback = None
global_time_delta = 0.25
Ping_MAX_AGE      = 3.0

_update_ship_sources(model_state.ship)

def _push_initial_frame():
    pr = model_state.predator
    pred_src.data = dict(x=[pr.position[0]], y=[pr.position[1]], c=["#FFFFFF"], s=[10.0])
    if model_state.preys:
        crew_src.data = dict(
            x=[m.position[0] for m in model_state.preys],
            y=[m.position[1] for m in model_state.preys],
            c=["#B8B8B8"]*len(model_state.preys),
            s=[5.0]*len(model_state.preys),
        )
    trail_src.data = dict(x=[], y=[])
    pings_src.data = dict(x=[], y=[], s=[])

_push_initial_frame()

# ─── Main loop ────────────────────────────────────────────────────────────────
def run_model():
    if not model_state.preys:
        periodic_callback.stop(); play_button.name = "▶ Play"; return

    # Read direction from mechanism A (JS→Bokeh model-sync) first;
    # fall back to mechanism B (Bokeh KeyDown events) if A is zero.
    js_dx = float(kbd_src.data.get('dx', [0.0])[0])
    js_dy = float(kbd_src.data.get('dy', [0.0])[0])
    dx = js_dx if (js_dx or js_dy) else _dir[0]
    dy = js_dy if (js_dx or js_dy) else _dir[1]

    model_state.predator.set_velocity(dx, dy)
    model_state.update(global_time_delta)
    pr = model_state.predator

    # Predator dot
    if pr.is_hatching:
        pulse = 0.5 + 0.5*math.sin(model_state.time*6.0)
        pc, ps = "#FFFFFF", 6.0+pulse*6.0
    else:
        pc = "#FFE020" if pr.locked_on else "#FF2244"
        ps = min(8.0+int(pr.eaten/150)*3.0, 26.0)
    pred_src.data = dict(x=[pr.position[0]], y=[pr.position[1]], c=[pc], s=[ps])

    # Crew
    if model_state.preys:
        crew_src.data = dict(
            x=[m.position[0] for m in model_state.preys],
            y=[m.position[1] for m in model_state.preys],
            c=[STATE_COLOR.get(m.state,"#888") for m in model_state.preys],
            s=[5.0]*len(model_state.preys),
        )

    # Trails
    tx, ty, step = [], [], max(1, len(model_state.preys)//100)
    for i, m in enumerate(model_state.preys):
        if i%step==0:
            for pos in m.trail: tx.append(pos[0]); ty.append(pos[1])
    trail_src.data = dict(x=tx, y=ty)

    # Pings
    if model_state.pings:
        pings_src.data = dict(
            x=[pg.x for pg in model_state.pings],
            y=[pg.y for pg in model_state.pings],
            s=[4.0+pg.age/Ping_MAX_AGE*30.0 for pg in model_state.pings],
        )
    else:
        pings_src.data = dict(x=[], y=[], s=[])

    # HUD
    t = model_state.time
    time_box.value    = f"{t:.1f} s"
    caught_box.value  = str(model_state.score.crew_caught)
    escaped_box.value = str(model_state.score.crew_escaped)
    remain_box.value  = str(len(model_state.preys))
    eaten_box.value   = str(int(pr.eaten))
    sm                = max(1, int(pr.eaten//150))
    speed_box.value   = f"{pr.base_speed*sm:.1f} u/s (×{sm})"
    status_box.value  = ("🥚 HATCHING" if pr.is_hatching
                         else "🔒 LOCKED ON" if pr.locked_on
                         else "👁 HUNTING")
    key_box.value     = f"dx={dx:+.0f}  dy={dy:+.0f}"   # live diagnostic
    secs = max(0.0, 1600.0-t)
    if model_state.shuttle_open:
        shuttle_box.value = "⚠ OPEN — CREW ESCAPING"
    else:
        mm, ss = divmod(int(secs), 60)
        shuttle_box.value = f"OPENS IN {mm:02d}:{ss:02d}"

# ─── Controls ─────────────────────────────────────────────────────────────────
def play(event):
    global periodic_callback
    if periodic_callback is None or not periodic_callback.running:
        play_button.name = "⏹ Stop"
        periodic_callback = pn.state.add_periodic_callback(
            run_model, period=max(17, 1000//fps_slider.value))
    else:
        periodic_callback.stop(); play_button.name = "▶ Play"

def reset(event):
    global model_state, periodic_callback
    if periodic_callback and periodic_callback.running:
        periodic_callback.stop(); periodic_callback = None
        play_button.name = "▶ Play"
    config["seed"]            = seed_input.value
    config["number_of_preys"] = crew_count_input.value
    config["predator_speed"]  = speed_slider.value
    config["lock_on_radius"]  = lock_on_slider.value
    config["hatch_time"]      = hatch_slider.value
    with open(configuration_file, "w") as f: json.dump(config, f, indent=4)
    model_state = Model(str(configuration_file))
    _update_ship_sources(model_state.ship)
    _push_initial_frame()
    _pressed.clear(); _dir[0]=_dir[1]=0.0
    kbd_src.data = dict(dx=[0.0], dy=[0.0])

# ─── Widgets ──────────────────────────────────────────────────────────────────
time_box     = pn.widgets.TextInput(name="Elapsed",         disabled=True, value="0.0 s")
remain_box   = pn.widgets.TextInput(name="Crew remaining",  disabled=True)
caught_box   = pn.widgets.TextInput(name="Crew caught",     disabled=True, value="0")
escaped_box  = pn.widgets.TextInput(name="Crew escaped",    disabled=True, value="0")
eaten_box    = pn.widgets.TextInput(name="Total eaten",     disabled=True, value="1")
speed_box    = pn.widgets.TextInput(name="Xeno speed",      disabled=True)
status_box   = pn.widgets.TextInput(name="Status",          disabled=True, value="🥚 HATCHING")
shuttle_box  = pn.widgets.TextInput(name="Shuttle bay",     disabled=True)
key_box      = pn.widgets.TextInput(name="🎮 Input (diag)", disabled=True, value="dx=+0  dy=+0")

seed_input       = pn.widgets.IntInput(   name="Random seed",     value=config.get("seed",1337))
crew_count_input = pn.widgets.IntInput(   name="Crew count",      value=config.get("number_of_preys",50), start=5, end=500)
speed_slider     = pn.widgets.FloatSlider(name="Xeno base speed", start=2.0, end=25.0, value=config.get("predator_speed",15.0))
lock_on_slider   = pn.widgets.FloatSlider(name="Lock-on radius",  start=0.0, end=50.0, step=1.0, value=config.get("lock_on_radius",20.0))
hatch_slider     = pn.widgets.FloatSlider(name="Hatch delay (s)", start=0.0, end=60.0, step=1.0, value=config.get("hatch_time",20.0))
fps_slider       = pn.widgets.IntSlider(  name="FPS",             start=4,   end=60,   value=30)

play_button  = pn.widgets.Button(name="▶ Play",  button_type="success")
reset_button = pn.widgets.Button(name="⟳ Reset", button_type="warning")
play_button.on_click(play)
reset_button.on_click(reset)

description = pn.pane.Markdown("""
### Controls
WASD or Arrow keys — no click required.

| Key | Action |
|-----|--------|
| **W / ↑** | Move up |
| **S / ↓** | Move down |
| **A / ←** | Move left |
| **D / →** | Move right |

Watch **🎮 Input** in the sidebar — it shows the direction
being received each frame. If it changes when you press keys,
the bridge is working.

---
### Crew states
| Colour | State |
|--------|-------|
| ⬜ Grey | Idle |
| 🟠 Orange | Alerted |
| 🟡 Gold | Fleeing |
| 🟢 Green | Hiding |
| 🔵 Cyan | Escaping |

---
### Tips
- **White pulsing** = hatching (20 s); predator can't move yet.
- **Yellow** = lock-on; steers automatically.
- Dot grows every 75 crew eaten (+speed).
- Shuttle opens at t = 1600 s.
- Crew in green vent rooms are invisible to lock-on.
""")

stats_card = pn.Card(
    time_box, remain_box, caught_box, escaped_box,
    eaten_box, speed_box, status_box, shuttle_box, key_box,
    collapsible=False, title="Mission Data",
)
config_card = pn.Card(
    seed_input, crew_count_input, speed_slider, lock_on_slider,
    hatch_slider, fps_slider,
    pn.Row(reset_button, play_button),
    collapsible=False, title="Simulation Config",
)

pn.template.FastListTemplate(
    theme=pn.template.DarkTheme,
    header_background="#8B0000",
    accent_base_color="#708090",
    site="ALIEN SIM",
    title="Alien Sim — Player Controlled",
    main=[
        keyboard_js,
        pn.Row(pn.Column(plot_pane), pn.Column(description, width=260)),
    ],
    sidebar=[stats_card, config_card],
).servable()