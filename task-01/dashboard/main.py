"""Alien Sim — Player Controlled
Visualization-only Python layer. All simulation logic runs in C++.

Controls:  WASD / Arrow keys — steer the Xenomorph
           Yellow dot = lock-on active (auto-lunging at nearest crew)
           White pulsing dot = hatching phase (can't move yet)
"""
import json
import math
import pathlib

import holoviews as hv
import numpy as np
import pandas as pd
import panel as pn
from holoviews import opts

from .model import Model, CrewState

pn.extension("bokeh")
hv.extension("bokeh")

# ─── Paths / config ───────────────────────────────────────────────────────────
configuration_file = pathlib.Path(__file__).parents[1] / "data" / "config.json"
config = json.load(open(configuration_file))

WORLD = config.get("region_limit", 100)

# ─── Crew state → colour ──────────────────────────────────────────────────────
STATE_COLOR = {
    CrewState.IDLE:     "#B8B8B8",   # light grey
    CrewState.ALERTED:  "#FF8C00",   # orange
    CrewState.FLEEING:  "#FFD700",   # gold
    CrewState.HIDING:   "#228B22",   # forest green  (hidden from lock-on)
    CrewState.ESCAPING: "#00CED1",   # dark turquoise
}

# ─── Keyboard bridge (JS → hidden TextInput → Python) ────────────────────────
_dir = [0.0, 0.0]   # [dx, dy] updated by JS

key_input = pn.widgets.TextInput(placeholder="__KEYSTATE__", value="0,0", visible=False)

def _on_key(event):
    try:
        dx, dy = event.new.split(",")
        _dir[0] = float(dx); _dir[1] = float(dy)
    except Exception:
        _dir[0] = _dir[1] = 0.0

key_input.param.watch(_on_key, "value")

keyboard_js = pn.pane.HTML("""
<script>
(function () {
    const pressed = {};
    const TRACKED = new Set(
        ['w','a','s','d','ArrowUp','ArrowDown','ArrowLeft','ArrowRight']);

    function dir() {
        let dx = 0, dy = 0;
        if (pressed['a'] || pressed['ArrowLeft'])  dx -= 1;
        if (pressed['d'] || pressed['ArrowRight']) dx += 1;
        if (pressed['w'] || pressed['ArrowUp'])    dy += 1;
        if (pressed['s'] || pressed['ArrowDown'])  dy -= 1;
        return dx + ',' + dy;
    }

    function push() {
        const el = document.querySelector('input[placeholder="__KEYSTATE__"]');
        if (!el) return;
        const setter = Object.getOwnPropertyDescriptor(
            HTMLInputElement.prototype, 'value').set;
        setter.call(el, dir());
        el.dispatchEvent(new Event('input',  {bubbles:true}));
        el.dispatchEvent(new Event('change', {bubbles:true}));
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
})();
</script>
""", height=0, width=0, margin=0)

# ─── HoloViews pipes for dynamic layers ───────────────────────────────────────
EMPTY = pd.DataFrame({"x": [], "y": [], "c": [], "s": []})
EMPTY_TRAIL  = pd.DataFrame({"x": [], "y": []})
EMPTY_PINGS  = pd.DataFrame({"x": [], "y": [], "s": []})

pipe_pred  = hv.streams.Pipe(data=EMPTY)
pipe_crew  = hv.streams.Pipe(data=EMPTY)
pipe_trail = hv.streams.Pipe(data=EMPTY_TRAIL)
pipe_pings = hv.streams.Pipe(data=EMPTY_PINGS)

# ─── Static ship overlay (computed after first model init) ────────────────────
_ship_overlay = None

def _build_ship_overlay(ship):
    corr_rects, vent_rects, shuttle_rects, normal_rects = [], [], [], []

    # Corridors: each L-shaped corridor = 2 axis-aligned rectangles
    for c in ship.corridors:
        for ax, ay, bx, by in [(c.x1, c.y1, c.bx, c.by),
                                (c.bx, c.by, c.x2, c.y2)]:
            corr_rects.append((min(ax,bx)-c.hw, min(ay,by)-c.hw,
                                max(ax,bx)+c.hw, max(ay,by)+c.hw))

    for r in ship.rooms:
        rect = (r.cx-r.hw, r.cy-r.hh, r.cx+r.hw, r.cy+r.hh)
        if r.is_vent:        vent_rects.append(rect)
        elif r.is_shuttle_bay: shuttle_rects.append(rect)
        else:                normal_rects.append(rect)

    def rects(data, color, line_c="#000000", lw=0):
        if not data:
            data = [(0, 0, 0, 0)]    # dummy so HoloViews doesn't complain
        return hv.Rectangles(data).opts(
            color=color, line_color=line_c, line_width=lw,
            fill_alpha=1.0, tools=[])

    overlay = (
        rects(corr_rects,    "#111A22")                           # dark corridors
        * rects(normal_rects, "#1C2B3A", "#2A4060", 1)           # crew rooms
        * rects(vent_rects,   "#0D1F0D", "#1A3520", 1)           # ventilation ducts
        * rects(shuttle_rects,"#0D1A2E", "#1A3860", 1)           # shuttle bay
    )
    return overlay

# ─── Dynamic plot callbacks ───────────────────────────────────────────────────
def _plot_pred(data):
    return hv.Scatter(data, kdims=["x"], vdims=["y","c","s"]).opts(
        opts.Scatter(color="c", size="s", line_color="black",
                     line_width=0.5, tools=[]))

def _plot_crew(data):
    return hv.Scatter(data, kdims=["x"], vdims=["y","c","s"]).opts(
        opts.Scatter(color="c", size="s", alpha=0.85, tools=[]))

def _plot_trail(data):
    return hv.Scatter(data, kdims=["x"], vdims=["y"]).opts(
        opts.Scatter(color="#556677", size=2, alpha=0.30, tools=[]))

def _plot_pings(data):
    return hv.Scatter(data, kdims=["x"], vdims=["y","s"]).opts(
        opts.Scatter(color="#FF6600", size="s", alpha=0.20,
                     line_color="#FF6600", line_width=1, line_alpha=0.5,
                     tools=[]))

dmap_pred  = hv.DynamicMap(_plot_pred,  streams=[pipe_pred])
dmap_crew  = hv.DynamicMap(_plot_crew,  streams=[pipe_crew])
dmap_trail = hv.DynamicMap(_plot_trail, streams=[pipe_trail])
dmap_pings = hv.DynamicMap(_plot_pings, streams=[pipe_pings])

PLOT_OPTS = dict(
    xlim=(-WORLD, WORLD), ylim=(-WORLD, WORLD),
    height=620, width=620,
    xaxis=None, yaxis=None,
    bgcolor="#060D14",
    show_grid=False,
)

# ─── Model & simulation state ─────────────────────────────────────────────────
model_state      = Model(str(configuration_file))
periodic_callback = None
global_time_delta = 0.25

# Build static ship overlay once
_ship_overlay = _build_ship_overlay(model_state.ship)

# Full plot: ship layout + dynamic layers
full_plot = (
    _ship_overlay
    * dmap_trail
    * dmap_crew
    * dmap_pings
    * dmap_pred
).opts(**PLOT_OPTS)

# ─── run_model (called every frame) ───────────────────────────────────────────
def run_model():
    if not model_state.preys:
        periodic_callback.stop()
        play_button.name = "▶ Play"
        return

    # Push player direction to C++ predator
    model_state.predator.set_velocity(_dir[0], _dir[1])
    model_state.update(global_time_delta)

    p = model_state.predator

    # ── Predator dot ──────────────────────────────────────────────────────
    if p.is_hatching:
        # Pulse white during egg phase
        pulse = 0.5 + 0.5 * math.sin(model_state.time * 6.0)
        pred_color = "#FFFFFF"
        pred_size  = 6.0 + pulse * 6.0
    else:
        pred_color = "#FFE020" if p.locked_on else "#FF2244"
        pred_size  = min(8.0 + int(p.eaten / 75) * 3.0, 26.0)

    pred_df = pd.DataFrame({"x": [p.position[0]], "y": [p.position[1]],
                             "c": [pred_color],    "s": [pred_size]})

    # ── Crew dots (coloured by state) ────────────────────────────────────
    if model_state.preys:
        crew_rows = [
            {"x": pr.position[0], "y": pr.position[1],
             "c": STATE_COLOR.get(pr.state, "#888888"), "s": 5.0}
            for pr in model_state.preys
        ]
        crew_df = pd.DataFrame(crew_rows)
    else:
        crew_df = EMPTY

    # ── O2 trails (sub-sampled for performance) ───────────────────────────
    trail_rows = []
    step = max(1, len(model_state.preys) // 150)
    for i, pr in enumerate(model_state.preys):
        if i % step == 0:
            for pos in pr.trail:
                trail_rows.append({"x": pos[0], "y": pos[1]})
    trail_df = pd.DataFrame(trail_rows) if trail_rows else EMPTY_TRAIL

    # ── Alert pings (growing rings) ───────────────────────────────────────
    if model_state.pings:
        ping_rows = [
            {"x": pg.x, "y": pg.y,
             "s": 4.0 + pg.age / Ping_MAX_AGE * 30.0}
            for pg in model_state.pings
        ]
        pings_df = pd.DataFrame(ping_rows)
    else:
        pings_df = EMPTY_PINGS

    pipe_pred.send(pred_df)
    pipe_crew.send(crew_df)
    pipe_trail.send(trail_df)
    pipe_pings.send(pings_df)

    # ── HUD ───────────────────────────────────────────────────────────────
    t = model_state.time
    time_box.value    = f"{t:.1f} s"
    caught_box.value  = str(model_state.score.crew_caught)
    escaped_box.value = str(model_state.score.crew_escaped)
    remain_box.value  = str(len(model_state.preys))
    eaten_box.value   = str(int(p.eaten))
    speed_mult        = max(1, int(p.eaten // 75))
    speed_box.value   = f"{p.base_speed * speed_mult:.1f} u/s  (×{speed_mult})"
    status_box.value  = ("🥚 HATCHING" if p.is_hatching
                         else "🔒 LOCKED ON" if p.locked_on
                         else "👁 HUNTING")

    secs_left = max(0.0, 1600.0 - t)
    if model_state.shuttle_open:
        shuttle_box.value = "⚠ OPEN — CREW ESCAPING"
    else:
        m, s = divmod(int(secs_left), 60)
        shuttle_box.value = f"OPENS IN {m:02d}:{s:02d}"


# Grab the Ping MAX_AGE constant from Python side
Ping_MAX_AGE = 3.0   # matches C++ Ping::MAX_AGE


# ─── Play / reset controls ────────────────────────────────────────────────────
def play(event):
    global periodic_callback
    if periodic_callback is None or not periodic_callback.running:
        play_button.name = "⏹ Stop"
        periodic_callback = pn.state.add_periodic_callback(
            run_model, period=max(17, 1000 // fps_slider.value))
    else:
        periodic_callback.stop()
        play_button.name = "▶ Play"


def reset(event):
    global model_state, _ship_overlay, periodic_callback

    if periodic_callback and periodic_callback.running:
        periodic_callback.stop(); periodic_callback = None
        play_button.name = "▶ Play"

    # Write updated config
    config["seed"]            = seed_input.value
    config["number_of_preys"] = crew_count_input.value
    config["predator_speed"]  = speed_slider.value
    config["lock_on_radius"]  = lock_on_slider.value
    config["hatch_time"]      = hatch_slider.value
    with open(configuration_file, "w") as f:
        json.dump(config, f, indent=4)

    model_state   = Model(str(configuration_file))
    _ship_overlay = _build_ship_overlay(model_state.ship)

    # Rebuild the full_plot overlay with the new ship layout
    new_plot = (
        _ship_overlay * dmap_trail * dmap_crew * dmap_pings * dmap_pred
    ).opts(**PLOT_OPTS)
    plot_pane.object = new_plot

    # Push initial frame
    _push_initial_frame()


def _push_initial_frame():
    p = model_state.predator
    pred_df = pd.DataFrame({"x": [p.position[0]], "y": [p.position[1]],
                             "c": ["#FFFFFF"], "s": [10.0]})
    if model_state.preys:
        crew_df = pd.DataFrame({
            "x": [pr.position[0] for pr in model_state.preys],
            "y": [pr.position[1] for pr in model_state.preys],
            "c": ["#B8B8B8"] * len(model_state.preys),
            "s": [5.0]       * len(model_state.preys),
        })
    else:
        crew_df = EMPTY
    pipe_pred.send(pred_df)
    pipe_crew.send(crew_df)
    pipe_trail.send(EMPTY_TRAIL)
    pipe_pings.send(EMPTY_PINGS)


_push_initial_frame()

# ─── Widgets ──────────────────────────────────────────────────────────────────
time_box     = pn.widgets.TextInput(name="Elapsed",        disabled=True, value="0.0 s")
remain_box   = pn.widgets.TextInput(name="Crew remaining", disabled=True)
caught_box   = pn.widgets.TextInput(name="Crew caught",    disabled=True, value="0")
escaped_box  = pn.widgets.TextInput(name="Crew escaped",   disabled=True, value="0")
eaten_box    = pn.widgets.TextInput(name="Total eaten",    disabled=True, value="1")
speed_box    = pn.widgets.TextInput(name="Xeno speed",     disabled=True)
status_box   = pn.widgets.TextInput(name="Status",         disabled=True, value="🥚 HATCHING")
shuttle_box  = pn.widgets.TextInput(name="Shuttle bay",    disabled=True)

seed_input       = pn.widgets.IntInput(   name="Random seed",      value=config.get("seed", 1337))
crew_count_input = pn.widgets.IntInput(   name="Crew count",       value=config.get("number_of_preys", 50), start=5, end=500)
speed_slider     = pn.widgets.FloatSlider(name="Xeno base speed",  start=2.0, end=25.0, value=config.get("predator_speed", 15.0))
lock_on_slider   = pn.widgets.FloatSlider(name="Lock-on radius",   start=0.0, end=50.0, step=1.0, value=config.get("lock_on_radius", 20.0))
hatch_slider     = pn.widgets.FloatSlider(name="Hatch delay (s)",  start=0.0, end=60.0, step=1.0, value=config.get("hatch_time", 20.0))
fps_slider       = pn.widgets.IntSlider(  name="FPS",              start=4,   end=60,   value=30)

play_button  = pn.widgets.Button(name="▶ Play",  button_type="success")
reset_button = pn.widgets.Button(name="⟳ Reset", button_type="warning")
play_button.on_click(play)
reset_button.on_click(reset)

plot_pane = pn.pane.HoloViews(full_plot, sizing_mode="fixed")

description = pn.pane.Markdown("""
### Controls
| Key | Action |
|-----|--------|
| **W / ↑** | Move up |
| **S / ↓** | Move down |
| **A / ←** | Move left |
| **D / →** | Move right |

---
### Crew states
| Colour | State |
|--------|-------|
| ⬜ Grey | Idle |
| 🟠 Orange | Alerted |
| 🟡 Gold | Fleeing to vent |
| 🟢 Green | Hiding (invisible to lock-on) |
| 🔵 Cyan | Escaping to shuttle |

---
### Tips
- **Yellow dot** = lock-on engaged; no input needed.
- **White pulsing dot** = alien still hatching.
- Dot **grows** each time you eat 75 crew and gains a speed bonus.
- Orange rings = alert pings (crew spotted you).
- Grey dots = O₂ trail left by crew.
- Shuttle bay **(top-right, blue room)** opens at t = 1600 s.
- Vent rooms **(green)** hide crew from your lock-on sensor.
""")

# ─── Layout ───────────────────────────────────────────────────────────────────
stats_card = pn.Card(
    time_box, remain_box, caught_box, escaped_box,
    eaten_box, speed_box, status_box, shuttle_box,
    collapsible=False, title="Mission Data",
)
config_card = pn.Card(
    seed_input, crew_count_input,
    speed_slider, lock_on_slider, hatch_slider,
    fps_slider,
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
        key_input,
        pn.Row(
            pn.Column(plot_pane),
            pn.Column(description, width=260),
        ),
    ],
    sidebar=[stats_card, config_card],
).servable()
