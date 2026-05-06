"""Alien Predator-Prey Simulation"""
import json
import pathlib

import holoviews as hv
import pandas as pd
import panel as pn

from .model import Model

hv.extension('bokeh')

# ── Config ────────────────────────────────────────────────────────────────────
configuration_file = pathlib.Path(__file__).parents[1] / 'data' / 'config.json'
config = json.load(open(configuration_file))
region_limit = config['region_limit']

# ── Colour maps ───────────────────────────────────────────────────────────────
PREY_COLORS  = {0: 'white', 1: 'yellow', 2: 'cyan'}   # Normal / Fleeing / Escaping
STAGE_COLORS = {0: '#ff4444', 1: '#991111', 2: '#330000'}  # Stalking / Hunting / Apex
STAGE_NAMES  = {0: 'Stalking', 1: 'Hunting', 2: 'Apex'}
ESCAPE_X, ESCAPE_Y = 90, 50

# ── Scatter view ──────────────────────────────────────────────────────────────
def plot_model(data):
    return hv.Scatter(data, kdims=['x'], vdims=['y', 'c']).opts(color='c', size=4)

scatter_pipe = hv.streams.Pipe(data=pd.DataFrame({'x': [], 'y': [], 'c': []}))

escape_marker = hv.Points([(ESCAPE_X, ESCAPE_Y)]).opts(
    color='lime', size=14, marker='triangle',
    line_color='white', line_width=1.5,
)

dmap = hv.DynamicMap(plot_model, streams=[scatter_pipe]).opts(
    xlim=(-region_limit, region_limit),
    ylim=(-region_limit, region_limit),
    height=600, width=600,
    bgcolor='#111111',
    title='Simulation View',
)
sim_plot = dmap * escape_marker   # overlay escape zone on live sim

# ── Population chart ──────────────────────────────────────────────────────────
pop_pipe = hv.streams.Pipe(data=pd.DataFrame({'time': [], 'prey': [], 'predators': []}))

def plot_population(data):
    opts = dict(height=250, width=600, bgcolor='#111111',
                xlabel='Time (s)', ylabel='Count',
                title='Population Over Time')
    if data.empty:
        return hv.Curve([], kdims=['time'], vdims=['count']).opts(**opts)
    prey_curve = hv.Curve(data, kdims=['time'], vdims=['prey'],       label='Prey'      ).opts(color='white', line_width=2)
    pred_curve = hv.Curve(data, kdims=['time'], vdims=['predators'],  label='Predators' ).opts(color='red',   line_width=2)
    return (prey_curve * pred_curve).opts(**opts, legend_position='top_right')

pop_dmap = hv.DynamicMap(plot_population, streams=[pop_pipe])

# ── Simulation state ──────────────────────────────────────────────────────────
model_state = Model(str(configuration_file))
time_history, prey_history, predator_history = [], [], []
global_time       = 0.0
global_time_delta = 0.25
periodic_callback = None


def _build_scatter():
    pred_rows = [
        {'x': p.position[0], 'y': p.position[1], 'c': STAGE_COLORS[p.get_stage()]}
        for p in model_state.predators
    ]
    prey_rows = [
        {'x': pr.position[0], 'y': pr.position[1], 'c': PREY_COLORS[pr.state]}
        for pr in model_state.preys
    ]
    return pd.concat([pd.DataFrame(pred_rows), pd.DataFrame(prey_rows)], ignore_index=True)


def run_model():
    global global_time
    global_time += global_time_delta
    model_state.update(global_time_delta)

    # Scatter
    scatter_pipe.send(_build_scatter())

    # Population chart
    time_history.append(global_time)
    prey_history.append(len(model_state.preys))
    predator_history.append(len(model_state.predators))
    pop_pipe.send(pd.DataFrame({
        'time':       time_history,
        'prey':       prey_history,
        'predators':  predator_history,
    }))

    # Info panel
    time_box.value     = f'{global_time:.1f}'
    prey_box.value     = str(len(model_state.preys))
    predator_box.value = str(len(model_state.predators))
    escaped_box.value  = str(model_state.escaped)

    if model_state.predators:
        p0 = model_state.predators[0]
        stage_box.value = STAGE_NAMES[p0.get_stage()]
        speed_box.value = f'{p0.get_current_speed():.1f}'
        eaten_box.value = str(int(p0.eaten))

    if not model_state.preys:
        periodic_callback.stop()
        play_button.name = 'Play'


def play(event):
    global periodic_callback
    if periodic_callback is None or not periodic_callback.running:
        play_button.name = 'Stop'
        periodic_callback = pn.state.add_periodic_callback(
            run_model, period=1000 // int(fps_input.value)
        )
    else:
        play_button.name = 'Play'
        periodic_callback.stop()


def reset(event):
    global model_state, periodic_callback, global_time
    global time_history, prey_history, predator_history

    config['seed']                 = seed_input.value
    config['region_limit']         = region_limits_input.value
    config['number_of_preys']      = number_of_preys_input.value
    config['number_of_predators']  = number_of_predators_input.value
    config['predator_speed']       = predator_speed_slider.value
    config['prey_speed']           = prey_speed_slider.value
    config['enable_flocking']      = flocking_toggle.value
    config['reproduction_enabled'] = reproduction_toggle.value
    with open(configuration_file, 'w') as f:
        json.dump(config, f, indent=4)

    if periodic_callback:
        periodic_callback.stop()
        periodic_callback = None
    play_button.name = 'Play'

    global_time = 0.0
    time_history.clear()
    prey_history.clear()
    predator_history.clear()

    model_state = Model(str(configuration_file))
    scatter_pipe.send(_build_scatter())
    pop_pipe.send(pd.DataFrame({'time': [], 'prey': [], 'predators': []}))

    # Reset info boxes
    time_box.value = '0';  prey_box.value = str(len(model_state.preys))
    predator_box.value = str(len(model_state.predators));  escaped_box.value = '0'
    stage_box.value = 'Stalking';  speed_box.value = '-';  eaten_box.value = '0'


# ── Widgets ───────────────────────────────────────────────────────────────────
time_box       = pn.widgets.TextInput(name='Time (s)',          disabled=True)
predator_box   = pn.widgets.TextInput(name='Predators',         disabled=True)
prey_box       = pn.widgets.TextInput(name='Prey Remaining',    disabled=True)
escaped_box    = pn.widgets.TextInput(name='Prey Escaped',      disabled=True)
eaten_box      = pn.widgets.TextInput(name='Prey Eaten',        disabled=True)
stage_box      = pn.widgets.TextInput(name='Predator Stage',    disabled=True)
speed_box      = pn.widgets.TextInput(name='Predator Speed',    disabled=True)

seed_input               = pn.widgets.IntInput(   name='Random Seed',          value=config['seed'])
region_limits_input      = pn.widgets.FloatInput( name='Region Limit',         value=config['region_limit'])
number_of_preys_input    = pn.widgets.IntInput(   name='Number of Prey',       value=config['number_of_preys'])
number_of_predators_input= pn.widgets.IntInput(   name='Number of Predators',  value=config.get('number_of_predators', 1))
predator_speed_slider    = pn.widgets.FloatSlider(name='Predator Speed',  start=1.0, end=30.0, value=config['predator_speed'])
prey_speed_slider        = pn.widgets.FloatSlider(name='Prey Speed',      start=0.5, end=10.0, value=config.get('prey_speed', 1.0))
flocking_toggle          = pn.widgets.Toggle(     name='Enable Flocking',      value=config.get('enable_flocking', True))
reproduction_toggle      = pn.widgets.Toggle(     name='Enable Reproduction',  value=config.get('reproduction_enabled', True))
fps_input                = pn.widgets.IntSlider(  name='FPS', start=1, end=30, step=1, value=30)

play_button  = pn.widgets.Button(name='Play',  button_type='success')
reset_button = pn.widgets.Button(name='Reset', button_type='warning')
play_button.on_click(play)
reset_button.on_click(reset)

reset(None)   # initialise model and fire initial scatter

# ── Layout ────────────────────────────────────────────────────────────────────
sim_data_card = pn.Card(
    time_box, predator_box, prey_box, escaped_box,
    eaten_box, stage_box, speed_box,
    collapsible=False, title='Simulation Data'
)

sim_config_card = pn.Card(
    seed_input, region_limits_input,
    number_of_preys_input, number_of_predators_input,
    predator_speed_slider, prey_speed_slider,
    flocking_toggle, reproduction_toggle,
    fps_input, reset_button, play_button,
    collapsible=False, title='Simulation Controls'
)

description_pane = pn.pane.Markdown('''
### Legend
- 🔴 **Predator** — Stalking (dim) → Hunting → **Apex** (near-black)
- ⚪ **Prey** — Normal · 🟡 Fleeing · 🔵 Escaping to pod
- 🟢 **Escape zone** — Crew reaching here after t=1600 survive

### Controls
| Setting | Effect |
|---|---|
| Seed | Repeatable runs |
| Region Limit | World boundary |
| Prey Count | Starting crew size |
| Predator Count | Number of xenomorphs |
| Predator Speed | Max speed (scales with stage) |
| Prey Speed | Flee/escape multiplier |
| Flocking | Boids separation + cohesion |
| Reproduction | Safe survivors spawn offspring |
| FPS | Playback speed (4 ≈ real-time) |
''')

pn.template.FastListTemplate(
    theme=pn.template.DarkTheme,
    header_background='#1a1a2e',
    accent_base_color='#DC143C',
    site='Alien Sim',
    title='Alien Predator-Prey Simulation',
    main=[
        pn.Row(sim_plot, description_pane),
        pn.Row(pop_dmap),
    ],
    sidebar=[sim_data_card, sim_config_card],
).servable()