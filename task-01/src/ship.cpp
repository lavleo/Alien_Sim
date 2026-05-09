#include "ship.h"
#include <algorithm>
#include <queue>

// ─── generate ────────────────────────────────────────────────────────────────
void Ship::generate(std::mt19937& rng, double world_limit) {
    rooms.clear(); corridors.clear(); vent_room_ids.clear();

    constexpr int COLS = 3, ROWS = 3;
    const double zw = world_limit * 2.0 / COLS;   // zone width  (~66.7)
    const double zh = world_limit * 2.0 / ROWS;   // zone height (~66.7)

    std::uniform_real_distribution<> jitter(-8.0,  8.0);
    std::uniform_real_distribution<> rhw   (10.0, 18.0);   // room half-width
    std::uniform_real_distribution<> rhh   (10.0, 16.0);   // room half-height

    rooms.resize(COLS * ROWS);
    for (int row = 0; row < ROWS; ++row) {
        for (int col = 0; col < COLS; ++col) {
            int id = col + row * COLS;
            Room& r = rooms[id];
            r.id = id;
            r.cx = -world_limit + zw * (col + 0.5) + jitter(rng);
            r.cy = -world_limit + zh * (row + 0.5) + jitter(rng);
            r.hw = rhw(rng);
            r.hh = rhh(rng);
            // Keep rooms inside world bounds
            r.cx = std::clamp(r.cx, -world_limit + r.hw + 4.0, world_limit - r.hw - 4.0);
            r.cy = std::clamp(r.cy, -world_limit + r.hh + 4.0, world_limit - r.hh - 4.0);
        }
    }

    // Special rooms: shuttle top-right (id=8), vents bottom-left (0) and mid-left (3)
    rooms[8].is_shuttle_bay = true;   shuttle_room_id = 8;
    rooms[0].is_vent        = true;
    rooms[3].is_vent        = true;
    vent_room_ids            = {0, 3};

    constexpr double CW = 5.0;   // corridor half-width

    // Horizontal connections: (col, row) → (col+1, row)
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS-1; ++col) {
            int a = col + row*COLS, b = (col+1) + row*COLS;
            const Room& ra = rooms[a]; const Room& rb = rooms[b];
            corridors.push_back({a, b,
                ra.cx, ra.cy,         // start
                rb.cx, ra.cy,         // bend: horizontal first then snap-y
                rb.cx, rb.cy,         // end
                CW});
        }

    // Vertical connections: (col, row) → (col, row+1)
    for (int row = 0; row < ROWS-1; ++row)
        for (int col = 0; col < COLS; ++col) {
            int a = col + row*COLS, b = col + (row+1)*COLS;
            const Room& ra = rooms[a]; const Room& rb = rooms[b];
            corridors.push_back({a, b,
                ra.cx, ra.cy,         // start
                ra.cx, rb.cy,         // bend: vertical first then snap-x
                rb.cx, rb.cy,         // end
                CW});
        }
}

// ─── walkability ─────────────────────────────────────────────────────────────
bool Ship::is_walkable(double x, double y) const noexcept {
    for (const auto& r : rooms)    if (r.contains(x, y)) return true;
    for (const auto& c : corridors) if (c.contains(x, y)) return true;
    return false;
}

// Wall-sliding: try full move, then x-only, then y-only, then stay
std::array<double,2> Ship::resolve_movement(
    const std::array<double,2>& old_pos,
    const std::array<double,2>& new_pos) const noexcept
{
    if (is_walkable(new_pos[0], new_pos[1])) return new_pos;
    if (is_walkable(new_pos[0], old_pos[1])) return {new_pos[0], old_pos[1]};
    if (is_walkable(old_pos[0], new_pos[1])) return {old_pos[0], new_pos[1]};
    return old_pos;
}

// ─── spatial queries ─────────────────────────────────────────────────────────
std::array<double,2> Ship::room_center(int id) const noexcept {
    if (id < 0 || id >= (int)rooms.size()) return {0.0, 0.0};
    return {rooms[id].cx, rooms[id].cy};
}

int Ship::room_at(double x, double y) const noexcept {
    for (const auto& r : rooms) if (r.contains(x, y)) return r.id;
    return -1;
}

int Ship::nearest_room(double x, double y) const noexcept {
    int    best   = 0;
    double best_d = std::numeric_limits<double>::max();
    for (const auto& r : rooms) {
        double d = std::hypot(r.cx-x, r.cy-y);
        if (d < best_d) { best_d = d; best = r.id; }
    }
    return best;
}

// ─── BFS pathfinding on room graph ───────────────────────────────────────────
std::vector<std::array<double,2>> Ship::path_between_rooms(int from_id, int to_id) const {
    if (from_id == to_id || to_id < 0) return {room_center(to_id)};

    const int N = (int)rooms.size();
    std::vector<int>  prev     (N, -1);
    std::vector<int>  corr_used(N, -1);
    std::vector<bool> visited  (N, false);
    std::queue<int>   q;
    q.push(from_id);
    visited[from_id] = true;

    while (!q.empty()) {
        int cur = q.front(); q.pop();
        if (cur == to_id) break;
        for (int ci = 0; ci < (int)corridors.size(); ++ci) {
            const auto& c = corridors[ci];
            int nb = -1;
            if      (c.room_a == cur && !visited[c.room_b]) nb = c.room_b;
            else if (c.room_b == cur && !visited[c.room_a]) nb = c.room_a;
            if (nb >= 0) {
                visited[nb] = true;
                prev[nb]    = cur;
                corr_used[nb] = ci;
                q.push(nb);
            }
        }
    }

    if (prev[to_id] < 0) return {room_center(to_id)};    // no path — go direct

    // Reconstruct room sequence
    std::vector<int> path;
    for (int cur = to_id; cur != from_id; cur = prev[cur]) path.push_back(cur);
    path.push_back(from_id);
    std::reverse(path.begin(), path.end());

    // Convert to world waypoints: bend + destination centre for each hop
    std::vector<std::array<double,2>> wps;
    for (int i = 0; i+1 < (int)path.size(); ++i) {
        int ci = corr_used[path[i+1]];
        if (ci >= 0) {
            const auto& c = corridors[ci];
            // Forward (room_a→room_b): bend is (bx, by)
            // Reverse (room_b→room_a): the mirror bend is (x1, y2) i.e. (room_a.cx, room_b.cy)
            if (c.room_a == path[i]) wps.push_back({c.bx, c.by});
            else                     wps.push_back({c.x1, c.y2});
        }
        wps.push_back(room_center(path[i+1]));
    }
    return wps;
}
