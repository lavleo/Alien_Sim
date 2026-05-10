#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
namespace py = pybind11;

#include "model.h"
#include "prey.h"
#include "predator.h"
#include "ship.h"

PYBIND11_MODULE(model, m) {

    // ── Enums ─────────────────────────────────────────────────────────────
    py::enum_<CrewState>(m, "CrewState")
        .value("IDLE",     CrewState::IDLE)
        .value("ALERTED",  CrewState::ALERTED)
        .value("FLEEING",  CrewState::FLEEING)
        .value("HIDING",   CrewState::HIDING)
        .value("ESCAPING", CrewState::ESCAPING)
        .export_values();

    // ── Ship layout (read-only after init) ────────────────────────────────
    py::class_<Room>(m, "Room")
        .def_readonly("id",             &Room::id)
        .def_readonly("cx",             &Room::cx)
        .def_readonly("cy",             &Room::cy)
        .def_readonly("hw",             &Room::hw)
        .def_readonly("hh",             &Room::hh)
        .def_property_readonly("is_vent",        [](const Room& r){ return r.is_vent(); })
        .def_property_readonly("is_shuttle_bay", [](const Room& r){ return r.is_shuttle_bay(); });

    py::class_<Corridor>(m, "Corridor")
        .def_readonly("x1", &Corridor::x1)
        .def_readonly("y1", &Corridor::y1)
        .def_readonly("bx", &Corridor::bx)
        .def_readonly("by", &Corridor::by)
        .def_readonly("x2", &Corridor::x2)
        .def_readonly("y2", &Corridor::y2)
        .def_readonly("hw", &Corridor::hw);

    py::class_<Ship>(m, "Ship")
        .def_readonly("rooms",           &Ship::rooms)
        .def_readonly("corridors",       &Ship::corridors)
        .def_readonly("shuttle_room_id", &Ship::shuttle_room_id)
        .def_readonly("vent_room_ids",   &Ship::vent_room_ids);

    // ── Predator ──────────────────────────────────────────────────────────
    py::class_<Predator>(m, "Predator")
        .def_readwrite("position",        &Predator::position)
        .def_readwrite("velocity",        &Predator::velocity)
        .def_readwrite("base_speed",      &Predator::base_speed)
        .def_readwrite("lock_on_radius",  &Predator::lock_on_radius)
        .def_readwrite("hatch_time",      &Predator::hatch_time)
        .def_readwrite("locked_on",       &Predator::locked_on)
        .def_readwrite("is_hatching",     &Predator::is_hatching)
        .def_readwrite("eaten",           &Predator::eaten)
        .def("set_velocity", &Predator::set_velocity,
             py::arg("dx"), py::arg("dy"));

    // ── Prey (read-only: state, position, trail) ──────────────────────────
    py::class_<Prey>(m, "Prey")
        .def_readonly("position", &Prey::position)
        .def_readonly("state",    &Prey::state)
        .def_readonly("trail",    &Prey::trail);

    // ── Pings and score ───────────────────────────────────────────────────
    py::class_<Ping>(m, "Ping")
        .def_readonly("x",   &Ping::x)
        .def_readonly("y",   &Ping::y)
        .def_readonly("age", &Ping::age);

    py::class_<Score>(m, "Score")
        .def_readonly("crew_caught",  &Score::crew_caught)
        .def_readonly("crew_escaped", &Score::crew_escaped)
        .def_readonly("elapsed_time", &Score::elapsed_time);

    // ── Model ─────────────────────────────────────────────────────────────
    py::class_<Model>(m, "Model")
        .def(py::init<const std::string&>())
        .def("update",        &Model::update)
        .def_readwrite("predator",    &Model::predator)
        .def_readwrite("preys",       &Model::preys)
        .def_readonly("ship",         &Model::ship)
        .def_readonly("pings",        &Model::pings)
        .def_readonly("score",        &Model::score)
        .def_readonly("shuttle_open", &Model::shuttle_open)
        .def_readonly("time",         &Model::time);
}
