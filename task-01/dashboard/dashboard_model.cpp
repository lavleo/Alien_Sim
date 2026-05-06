#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
namespace py = pybind11;

#include "model.h"
#include "prey.h"
#include "predator.h"

PYBIND11_MODULE(model, m) {
    py::class_<Model>(m, "Model")
        .def(py::init<const std::string&>())
        .def("update",  &Model::update)
        .def("reset",   &Model::reset)
        .def_readwrite("predators", &Model::predators)
        .def_readwrite("preys",     &Model::preys)
        .def_readwrite("escaped",   &Model::escaped)
        .def_readwrite("time",      &Model::time);

    py::class_<Predator>(m, "Predator")
        .def_readwrite("position", &Predator::position)
        .def_readwrite("eaten",    &Predator::eaten)
        .def_readwrite("speed",    &Predator::speed)
        .def("get_stage",         &Predator::get_stage)
        .def("get_current_speed", &Predator::get_current_speed);

    py::class_<Prey>(m, "Prey")
        .def_readwrite("position",   &Prey::position)
        .def_readwrite("state",      &Prey::state)
        .def_readwrite("speed",      &Prey::speed)
        .def_readwrite("time_alive", &Prey::time_alive);
}
