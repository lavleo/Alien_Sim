#pragma once
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

// ─── RoomType ─────────────────────────────────────────────────────────────────
enum class RoomType {
    Normal,
    Bridge,        // fore cap — command centre
    Medbay,        // fore section
    Comms,         // fore section
    Armory,        // mid section
    CrewQuarters,  // mid section
    Storage,       // mid section
    Reactor,       // aft section
    EngineRoom,    // stern — primary propulsion
    EnginePod,     // nacelle branches off aft/stern
    ShuttleBay,    // stern — escape craft dock
    VentShaft,     // narrow crawl-space room, dead-end
};

// ─── Room ─────────────────────────────────────────────────────────────────────
struct Room {
    int      id{};
    double   cx{}, cy{};   // centre
    double   hw{}, hh{};   // half-extents
    RoomType type{ RoomType::Normal };

    // Convenience queries — the rest of the codebase calls these as methods now
    bool is_vent()        const noexcept { return type == RoomType::VentShaft;  }
    bool is_shuttle_bay() const noexcept { return type == RoomType::ShuttleBay; }
    bool is_engine()      const noexcept { return type == RoomType::EngineRoom
                                               || type == RoomType::EnginePod;  }
    bool is_bridge()      const noexcept { return type == RoomType::Bridge;     }

    bool contains(double x, double y) const noexcept {
        return x >= cx-hw && x <= cx+hw && y >= cy-hh && y <= cy+hh;
    }
};

// ─── Corridor (L-shaped) ──────────────────────────────────────────────────────
struct Corridor {
    int    room_a{-1}, room_b{-1};
    double x1{}, y1{};        // start  (room_a centre)
    double bx{}, by{};        // bend point
    double x2{}, y2{};        // end    (room_b centre)
    double hw{5.0};           // half-width
    bool   is_vent_shaft{false};  // narrow crawl passage, non-standard traversal

    bool contains(double x, double y) const noexcept {
        auto seg = [&](double ax, double ay, double bbx, double bby) noexcept {
            double lx = std::min(ax,bbx)-hw, hx = std::max(ax,bbx)+hw;
            double ly = std::min(ay,bby)-hw, hy = std::max(ay,bby)+hw;
            return x>=lx && x<=hx && y>=ly && y<=hy;
        };
        return seg(x1,y1,bx,by) || seg(bx,by,x2,y2);
    }
};

// ─── ShipClass & ShipProfile ──────────────────────────────────────────────────
enum class ShipClass { Fighter, Frigate, Cruiser, Carrier };

struct ShipProfile {
    ShipClass klass;
    double    spine_length;      // nose-to-stern world units
    double    max_half_width;    // half-extent at the widest point
    int       room_count_min;
    int       room_count_max;
    bool      strict_symmetry;   // true = mirror branch rooms exactly

    // Hull taper: returns allowed half-width at normalised spine position
    // t=0 is the nose tip, t=1 is the stern.
    // Shape: sharp nose ramp → wide mid plateau → aft taper → nacelle bulge → narrow stern cap
    double taper(double t) const noexcept {
        const double w = max_half_width;
        if      (t < 0.12) return w * (t / 0.12);                              // nose ramp
        else if (t < 0.65) return w;                                            // mid plateau
        else if (t < 0.80) return w * (1.0 - 0.25*(t-0.65)/0.15);             // aft taper → 0.75w
        else if (t < 0.92) return w * (0.75 + 0.20*(t-0.80)/0.12);            // nacelle bulge → 0.95w
        else               return w * (0.95 * (1.0 - (t-0.92)/0.08));         // stern cap → 0
    }

    // Factory — construct a profile for a given class
    static ShipProfile make(ShipClass klass) noexcept {
        switch (klass) {
            case ShipClass::Fighter: return { klass, 120.0,  28.0,  5,  7, true  };
            case ShipClass::Frigate: return { klass, 180.0,  45.0,  8, 12, false };
            case ShipClass::Cruiser: return { klass, 240.0,  60.0, 13, 18, false };
            case ShipClass::Carrier: return { klass, 280.0,  80.0, 18, 26, true  };
        }
        return { ShipClass::Frigate, 180.0, 45.0, 8, 12, false }; // unreachable
    }
};

// ─── Ship ─────────────────────────────────────────────────────────────────────
class Ship {
public:
    ShipProfile           profile{ ShipProfile::make(ShipClass::Frigate) };
    std::vector<Room>     rooms;
    std::vector<Corridor> corridors;
    int                   shuttle_room_id{-1};
    std::vector<int>      vent_room_ids;

    // Convex hull polygon for schematic outline rendering
    std::vector<double>   hull_xs;
    std::vector<double>   hull_ys;

    // Build a procedurally generated ship; replaces the old 3×3 grid layout
    void generate(std::mt19937& rng, double world_limit = 100.0);

    bool                 is_walkable(double x, double y) const noexcept;
    std::array<double,2> resolve_movement(const std::array<double,2>& old_pos,
                                          const std::array<double,2>& new_pos) const noexcept;

    std::array<double,2> room_center(int id) const noexcept;
    int  room_at     (double x, double y) const noexcept;
    int  nearest_room(double x, double y) const noexcept;

    // BFS path on room graph → ordered world-space waypoints (bend + room centre)
    std::vector<std::array<double,2>> path_between_rooms(int from_id, int to_id) const;
};