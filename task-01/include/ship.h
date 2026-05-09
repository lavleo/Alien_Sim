#pragma once
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

// ─── Room ────────────────────────────────────────────────────────────────────
struct Room {
    int    id{};
    double cx{}, cy{};        // centre
    double hw{}, hh{};        // half-extents
    bool   is_vent{false};
    bool   is_shuttle_bay{false};

    bool contains(double x, double y) const noexcept {
        return x >= cx-hw && x <= cx+hw && y >= cy-hh && y <= cy+hh;
    }
};

// ─── Corridor (L-shaped) ─────────────────────────────────────────────────────
struct Corridor {
    int    room_a{-1}, room_b{-1};
    double x1{}, y1{};        // start  (room_a centre)
    double bx{}, by{};        // bend
    double x2{}, y2{};        // end    (room_b centre)
    double hw{5.0};           // half-width

    bool contains(double x, double y) const noexcept {
        auto seg = [&](double ax, double ay, double bbx, double bby) noexcept {
            double lx = std::min(ax,bbx)-hw, hx = std::max(ax,bbx)+hw;
            double ly = std::min(ay,bby)-hw, hy = std::max(ay,bby)+hw;
            return x>=lx && x<=hx && y>=ly && y<=hy;
        };
        return seg(x1,y1,bx,by) || seg(bx,by,x2,y2);
    }
};

// ─── Ship ─────────────────────────────────────────────────────────────────────
class Ship {
public:
    std::vector<Room>     rooms;
    std::vector<Corridor> corridors;
    int                   shuttle_room_id{8};
    std::vector<int>      vent_room_ids;

    // Build a 3×3 zone layout; assigns shuttle bay and vents
    void generate(std::mt19937& rng, double world_limit = 100.0);

    bool               is_walkable(double x, double y) const noexcept;
    std::array<double,2> resolve_movement(const std::array<double,2>& old_pos,
                                          const std::array<double,2>& new_pos) const noexcept;

    std::array<double,2> room_center(int id) const noexcept;
    int  room_at      (double x, double y) const noexcept;
    int  nearest_room (double x, double y) const noexcept;

    // BFS path on room graph → ordered world-space waypoints (bend + room centre)
    std::vector<std::array<double,2>> path_between_rooms(int from_id, int to_id) const;
};
