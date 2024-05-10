[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/DMBnbkTd)
[![Open in Codespaces](https://classroom.github.com/assets/launch-codespace-7f7980b617ed060a017424585567c406b6ee15c891e84e1186181d67ecf80aa0.svg)](https://classroom.github.com/open-in-codespaces?assignment_repo_id=14459134)
# cpp-final-project-predator-prey

A compiled sample simulation is provided under [`sample`](./sample) for you to get an idea of what this may look like. Simply run `make dashboard` to launch the simulation.

## Predator-Prey Model

The predator-prey model is a system of entities where at least one *predator* entity seeks out and "eats" *prey* entities. Your task is to implement a model from near-scratch, while following a few requirements for your classes. Due to the open nature of this, there is no autograder. There is no explicitly correct way to complete this project, only that it includes at least one predator seeking out and eating *many* other entities.

### Grading

You will be graded on the following criteria:

* Structure Adherance
* Runtime Performance
* Code Organization
* Implementation of features and behaviors

#### Core Requirements

* Code is submitted through GitHub
* A written report is required
  * This should be about a page, and should be provided as a file named REPORT.md with your code
  * This report should detail the project, challenges encountered, results, and possible improvements.
* Short 15-25 minute presentation of your project

| Scoring Criteria | 4 | 3 | 2 | 1 | 0 | Weight |
| ---------------- | --- | --- | --- | --- | --- | --- |
| Structure        | All Required Structures Implemented | N/A | N/A | N/A | Any Discrepancy | 2 |
| Performance Pt1. | 30FPS with 1000 Entities | 30FPS with 500 Entities | 30FPS with 250 Entities | N/A | <30FPS with 250 Entities | 8 |
| Performance Pt2. | High CPU, Memory Efficiency | Good Efficiency | OK Efficiency | Poor Efficiency | Inefficient | 5 |
| Organization     | Separate Pairs of Files for All Classes | N/A | N/A | N/A | Mixed, Large Files | 2 |
| Features         | 1 Advanced Feature | Prey Try to Avoid the Predator | N/A | Predator Eats Helpless Prey | Nothing Happens | 3 |
| Presentation     | Report is complete and presented | N/A | N/A | N/A | Missing Report/Presentation | 5 |

### Structure

In order for your code to work properly with the visualizer, you must implement the following classes with the following members. **NOTE: you will add many other functions and members to these classes; the following are just minimum requirements for the visualization**

#### Model

The `Model` class will be the primary class that houses and drives all simulation components. It must have the following members:

* A constructor `Model` that accepts a `const std::string&`
* A function `update` that accepts a `const double` and returns `void`
* A `Predator` named `predator`
* A `std::vector<Prey>` named `preys`

##### Model Constructor

This constructor must be implemented to accept a string and must use `nlohmann/json.hpp` for loading the simulation configuration from a file. This constructor will be responsible for taking the information from the configuration and initializing the simulation state.

#### Predator

The `Predator` class represents a predator in the model; there will be only one of these in your simulation. It must contain the following:

* A `std::array<double, 2>` named `position`

#### Prey

The `Prey` class represents a prey in the model; there will be **many** of these in your simulation. It must contain the following:

* A `std::array<double, 2>` named `position`

### Performance

Due to the wide-open nature of this project, there are no super strict guidelines here other than that your simulation must still be able to run at ~30FPS in a Codespace with at least 1000 prey entities. CPU and memory efficiency will absolutely influence your grade, even if your simulation meets the 30FPS requirement. Your implementation should leverage references, avoid copying data, minimize memory usage, efficiently iterate through loops and structures, and whatever else you can do to write an efficient program.

There are two compiler options (`-O3` and `-ffast-math`) enabled that will heavily optimize your programs for performance, but this is not a substitute for writing efficient and robust code. These options will allow us to simulate upwards 5000 entities with only *minor* slowdowns (assuming you've written efficient code)!

### Code Organization

Classes must be defined in separate files from each other, and furthermore classes must be split between `.cpp` and `.h` files. At a minumum this means having the following files:

* `model.cpp`
* `model.h`
* `predator.cpp`
* `predator.h`
* `prey.cpp`
* `prey.h`

Note that `main.cpp` is missing from this list. You are free to write a `main.cpp`, but it will not be necessary for the project (though it may be very useful for debugging!).

### Features & Behaviors

This is where your projects will have a lot of flexibility. You can choose how your entities move and behave. At a minimum the predator must be capable of catching prey assuming there is no interference. Prey should attempt to avoid the predator while also trying achieve some goal(s) (e.g. traveling to waypoints, moving resources from one area to another, etc.).

Here are some other ideas you may use (and definitely not limited to!) to enhance your model:

* terrain - regions where movement is modified (e.g. slowed down or sped up)
* traps - entities become immobilized for a short time if they move too close to some coordinate(s)
* obstacles/walls - small regions that cannot be occcupied/traversed by any entity

At this moment in time spawning multiple predators is not allowed (including converting prey to predators as if by vampirism).
