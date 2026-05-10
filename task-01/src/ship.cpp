#include "ship.h"
#include <algorithm>
#include <cassert>
#include <numeric>
#include <queue>
#include <set>

// ═════════════════════════════════════════════════════════════════════════════
// Internal helpers (anonymous namespace — not visible outside this TU)
// ═════════════════════════════════════════════════════════════════════════════
namespace {

// ─── Room size tables ────────────────────────────────────────────────────────
struct SizeRange { double hw_min, hw_max, hh_min, hh_max; };

SizeRange size_for(RoomType t) noexcept {
    switch (t) {
        case RoomType::Bridge:        return {14, 20, 11, 16};
        case RoomType::Medbay:        return {17, 23, 14, 20};
        case RoomType::Comms:         return {14, 20, 14, 18};
        case RoomType::Armory:        return {15, 21, 14, 18};
        case RoomType::CrewQuarters:  return {18, 26, 16, 23};
        case RoomType::Storage:       return {17, 24, 14, 20};
        case RoomType::Reactor:       return {20, 26, 17, 23};
        case RoomType::EngineRoom:    return {21, 28, 17, 23};
        case RoomType::EnginePod:     return {28, 40, 11, 17}; // wide, squat
        case RoomType::ShuttleBay:    return {22, 31, 18, 26};
        case RoomType::VentShaft:     return { 8, 14,  8, 13};
        default:                      return {17, 24, 14, 21};
    }
}

Room make_room(int id, double cx, double cy, RoomType type, std::mt19937& rng) {
    auto [hw_min, hw_max, hh_min, hh_max] = size_for(type);
    Room r;
    r.id   = id;
    r.cx   = cx;
    r.cy   = cy;
    r.hw   = std::uniform_real_distribution<>(hw_min, hw_max)(rng);
    r.hh   = std::uniform_real_distribution<>(hh_min, hh_max)(rng);
    r.type = type;
    return r;
}

// ─── Zone-aware room type selection ──────────────────────────────────────────
// t = 0 (nose) … 1 (stern), is_branch = room off to the side of spine
RoomType pick_room_type(double t, bool is_branch, std::mt19937& rng) {
    if (t < 0.12)                    return RoomType::Bridge;
    if (t > 0.85)                    return RoomType::EngineRoom;

    std::uniform_real_distribution<> roll(0.0, 1.0);
    double r = roll(rng);

    if (t < 0.35) {          // Fore section
        return is_branch ? (r < 0.5 ? RoomType::Medbay  : RoomType::Comms)
                         : (r < 0.6 ? RoomType::Comms   : RoomType::Medbay);
    }
    if (t < 0.65) {          // Mid section
        if (is_branch) {
            if (r < 0.35) return RoomType::CrewQuarters;
            if (r < 0.65) return RoomType::Storage;
            return RoomType::Armory;
        }
        return r < 0.5 ? RoomType::Armory : RoomType::Storage;
    }
    // Aft section
    if (is_branch) return r < 0.6 ? RoomType::EnginePod : RoomType::Reactor;
    return RoomType::Reactor;
}

// ─── Grammar rules ────────────────────────────────────────────────────────────
// Each spine node fires one of these rules to determine what rooms it spawns
enum class Rule { SpineRoom, BranchPair, SpineAndWings, Skip };

Rule pick_rule(double t, bool is_first, bool is_last, std::mt19937& rng) {
    if (is_first || is_last) return Rule::SpineRoom; // nose/stern are always on-spine

    std::uniform_real_distribution<> roll(0.0, 1.0);
    double r = roll(rng);

    if (t < 0.35) {          // Fore: mostly spine rooms, some branches
        if (r < 0.40) return Rule::SpineRoom;
        if (r < 0.75) return Rule::BranchPair;
        return Rule::Skip;
    }
    if (t < 0.65) {          // Mid: branches dominate, wings common
        if (r < 0.15) return Rule::SpineRoom;
        if (r < 0.50) return Rule::BranchPair;
        if (r < 0.85) return Rule::SpineAndWings;
        return Rule::Skip;
    }
    // Aft: spine rooms with some branch pods
    if (r < 0.40) return Rule::SpineRoom;
    if (r < 0.75) return Rule::BranchPair;
    return Rule::SpineAndWings;
}

// ─── Overlap resolution ───────────────────────────────────────────────────────
// Axis-aligned push-apart on minimum-overlap axis
void push_apart(Room& a, Room& b, double pad = 3.0) noexcept {
    double dx = b.cx - a.cx;
    double dy = b.cy - a.cy;
    double ox = (a.hw + b.hw + pad) - std::abs(dx);  // overlap on x
    double oy = (a.hh + b.hh + pad) - std::abs(dy);  // overlap on y
    if (ox <= 0 || oy <= 0) return;  // no overlap
    if (ox < oy) {
        double push = ox * 0.5;
        int    sign = (dx >= 0) ? 1 : -1;
        a.cx -= sign * push;
        b.cx += sign * push;
    } else {
        double push = oy * 0.5;
        int    sign = (dy >= 0) ? 1 : -1;
        a.cy -= sign * push;
        b.cy += sign * push;
    }
}

bool rooms_overlap(const Room& a, const Room& b, double pad = 2.0) noexcept {
    return std::abs(a.cx - b.cx) < (a.hw + b.hw + pad) &&
           std::abs(a.cy - b.cy) < (a.hh + b.hh + pad);
}

// ─── Convex hull (Graham scan) ───────────────────────────────────────────────
std::vector<std::array<double,2>> convex_hull(std::vector<std::array<double,2>> pts) {
    int n = (int)pts.size();
    if (n < 3) return pts;

    // Find lowest-then-leftmost pivot
    int pivot = 0;
    for (int i = 1; i < n; ++i)
        if (pts[i][1] < pts[pivot][1] ||
            (pts[i][1] == pts[pivot][1] && pts[i][0] < pts[pivot][0]))
            pivot = i;
    std::swap(pts[0], pts[pivot]);

    double px = pts[0][0], py = pts[0][1];
    std::sort(pts.begin()+1, pts.end(), [px,py](const auto& a, const auto& b){
        double ax = a[0]-px, ay = a[1]-py;
        double bx = b[0]-px, by = b[1]-py;
        double cross = ax*by - ay*bx;
        if (std::abs(cross) > 1e-9) return cross > 0.0;
        return ax*ax+ay*ay < bx*bx+by*by;
    });

    std::vector<std::array<double,2>> hull;
    hull.reserve(n);
    for (auto& p : pts) {
        while (hull.size() >= 2) {
            const auto& a = hull[hull.size()-2];
            const auto& b = hull[hull.size()-1];
            double cross = (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0]);
            if (cross <= 0.0) hull.pop_back();
            else break;
        }
        hull.push_back(p);
    }
    return hull;
}

// ─── Corridor helper ──────────────────────────────────────────────────────────
// Builds an L-shaped corridor between two rooms.
// horizontal_first=true → bend goes horizontal then vertical (for branch corridors)
// horizontal_first=false → bend goes vertical then horizontal (for spine corridors)
Corridor make_corridor(int a, int b, const Room& ra, const Room& rb,
                        double hw, bool horizontal_first, bool is_vent = false) {
    Corridor c;
    c.room_a       = a;
    c.room_b       = b;
    c.x1           = ra.cx;  c.y1 = ra.cy;
    c.x2           = rb.cx;  c.y2 = rb.cy;
    c.hw           = hw;
    c.is_vent_shaft = is_vent;
    if (horizontal_first) { c.bx = rb.cx; c.by = ra.cy; }
    else                  { c.bx = ra.cx; c.by = rb.cy; }
    return c;
}

} // anonymous namespace

// ═════════════════════════════════════════════════════════════════════════════
// Ship::generate — full procedural pipeline
// ═════════════════════════════════════════════════════════════════════════════
void Ship::generate(std::mt19937& rng, double world_limit) {
    rooms.clear(); corridors.clear();
    vent_room_ids.clear(); shuttle_room_id = -1;
    hull_xs.clear(); hull_ys.clear();

    // ── Stage 1: Profile ──────────────────────────────────────────────────────
    // Always use the largest ship class for a bigger, harder-to-hunt layout
    ShipClass klass = ShipClass::Carrier;
    profile = ShipProfile::make(klass);

    // Scale profile so the spine fills the world generously (was 1.7x, now 2.1x)
    double scale = std::min(1.0, (world_limit * 2.1) / profile.spine_length);
    profile.spine_length   *= scale;
    profile.max_half_width *= scale;

    std::uniform_int_distribution<> count_dist(profile.room_count_min,
                                                profile.room_count_max);
    int target_rooms = count_dist(rng);

    // ── Stage 2: Spine nodes ─────────────────────────────────────────────────
    // Number of nodes ≈ 2/3 of target rooms (branches add more per node)
    int spine_n = std::max(4, target_rooms * 2 / 3);

    double nose_y  =  profile.spine_length * 0.5;   // top   (high Y)
    double stern_y = -profile.spine_length * 0.5;   // bottom (low Y)

    struct SpineNode { double x, y, t; };
    std::vector<SpineNode> nodes(spine_n);

    for (int i = 0; i < spine_n; ++i) {
        double t = (double)i / (spine_n - 1);
        double y = nose_y - t * profile.spine_length;
        // Small lateral spine drift — stays within 12% of local taper width
        double max_jit = profile.taper(t) * 0.12;
        double x = std::uniform_real_distribution<>(-max_jit, max_jit)(rng);
        nodes[i] = {x, y, t};
    }

    // ── Stage 3: Grammar pass ─────────────────────────────────────────────────
    // For each spine node, fire a grammar rule and spawn rooms accordingly.
    // We track per-node slot info so the corridor pass can route properly.

    struct Slot {
        int spine_id{-1};   // room sitting on the spine
        int left_id {-1};   // left  branch room
        int right_id{-1};   // right branch room
    };
    std::vector<Slot> slots(spine_n);
    int next_id = 0;

    // primary[i]: the "representative" room for spine node i used for chaining.
    // For SpineRoom/SpineAndWings this is spine_id.
    // For BranchPair (no on-spine room) this is left_id (arbitrary; we'll still
    // connect both branches into the spine chain via the corridor pass).
    std::vector<int> primary(spine_n, -1);

    for (int i = 0; i < spine_n; ++i) {
        const auto& nd  = nodes[i];
        bool is_first   = (i == 0);
        bool is_last    = (i == spine_n - 1);
        Rule rule       = pick_rule(nd.t, is_first, is_last, rng);
        double allowed  = profile.taper(nd.t);

        // ── On-spine room ───────────────────────────────────────────────────
        if (rule == Rule::SpineRoom || rule == Rule::SpineAndWings) {
            RoomType rt = is_first ? RoomType::Bridge
                        : is_last  ? RoomType::EngineRoom
                        : pick_room_type(nd.t, false, rng);
            rooms.push_back(make_room(next_id, nd.x, nd.y, rt, rng));
            slots[i].spine_id = next_id;
            primary[i]        = next_id++;
        }

        // ── Branch pair ─────────────────────────────────────────────────────
        if (rule == Rule::BranchPair || rule == Rule::SpineAndWings) {
            RoomType rt   = (nd.t > 0.65) ? RoomType::EnginePod
                                           : pick_room_type(nd.t, true, rng);
            SizeRange sz  = size_for(rt);
            double typ_hw = (sz.hw_min + sz.hw_max) * 0.5;

            // Branch offset: far enough to not overlap the spine room
            double offset = std::max(typ_hw + 6.0, allowed * 0.55);
            double yj     = std::uniform_real_distribution<>(-3.0, 3.0)(rng);

            // Left branch
            rooms.push_back(make_room(next_id, nd.x - offset, nd.y + yj, rt, rng));
            slots[i].left_id = next_id++;

            // Right branch — strict symmetry = exact mirror; loose = independent
            if (profile.strict_symmetry) {
                Room mirror   = rooms.back();
                mirror.id     = next_id;
                mirror.cx     = nd.x + offset;
                rooms.push_back(mirror);
            } else {
                rooms.push_back(make_room(next_id, nd.x + offset, nd.y + yj, rt, rng));
            }
            slots[i].right_id = next_id++;

            if (primary[i] < 0) primary[i] = slots[i].left_id; // BranchPair primary
        }
        // Rule::Skip leaves primary[i] = -1; the corridor pass will bridge over it
    }

    // ── Stage 4: Mandatory stern rooms ───────────────────────────────────────
    // Shuttle bay: a separate room placed below the engine room at the stern tip
    {
        const auto& last = nodes[spine_n - 1];
        double eng_hh    = (slots[spine_n-1].spine_id >= 0)
                           ? rooms[slots[spine_n-1].spine_id].hh : 14.0;
        double sba_cy    = last.y - eng_hh - 20.0;
        rooms.push_back(make_room(next_id, last.x, sba_cy, RoomType::ShuttleBay, rng));
        shuttle_room_id  = next_id++;
    }

    // ── Stage 5: Clamp all rooms inside world bounds ──────────────────────────
    for (auto& r : rooms) {
        r.cx = std::clamp(r.cx, -world_limit + r.hw + 3.0, world_limit - r.hw - 3.0);
        r.cy = std::clamp(r.cy, -world_limit + r.hh + 3.0, world_limit - r.hh - 3.0);
    }

    // Iterative push-apart to resolve overlaps (4 passes is enough for <30 rooms)
    for (int iter = 0; iter < 4; ++iter)
        for (int a = 0; a < (int)rooms.size(); ++a)
            for (int b = a+1; b < (int)rooms.size(); ++b)
                if (rooms_overlap(rooms[a], rooms[b], 3.0))
                    push_apart(rooms[a], rooms[b], 3.0);

    // Re-clamp after push-apart
    for (auto& r : rooms) {
        r.cx = std::clamp(r.cx, -world_limit + r.hw + 3.0, world_limit - r.hw - 3.0);
        r.cy = std::clamp(r.cy, -world_limit + r.hh + 3.0, world_limit - r.hh - 3.0);
    }

    // ── Stage 6: Corridor routing ─────────────────────────────────────────────
    constexpr double MAIN_HW = 5.0;   // standard corridor half-width
    constexpr double VENT_HW = 2.0;   // vent shaft half-width

    // Fill gaps: for Skip nodes, inherit primary from the nearest filled node
    // Forward fill first, then backward fill
    std::vector<int> anchor = primary;
    for (int i = 1; i < spine_n; ++i)
        if (anchor[i] < 0) anchor[i] = anchor[i-1];
    for (int i = spine_n-2; i >= 0; --i)
        if (anchor[i] < 0) anchor[i] = anchor[i+1];

    // (a) Spine chain — connect consecutive anchor rooms top to bottom
    for (int i = 0; i+1 < spine_n; ++i) {
        if (anchor[i] < 0 || anchor[i+1] < 0 || anchor[i] == anchor[i+1]) continue;
        corridors.push_back(make_corridor(
            anchor[i], anchor[i+1],
            rooms[anchor[i]], rooms[anchor[i+1]],
            MAIN_HW, false));  // vertical-first bend for spine
    }

    // (b) Branch corridors — connect left/right rooms to their spine anchor
    for (int i = 0; i < spine_n; ++i) {
        int anch = anchor[i];
        if (anch < 0) continue;

        for (int br_id : { slots[i].left_id, slots[i].right_id }) {
            if (br_id < 0 || br_id == anch) continue;
            corridors.push_back(make_corridor(
                anch, br_id,
                rooms[anch], rooms[br_id],
                MAIN_HW, true));  // horizontal-first bend for branches
        }

        // BranchPair with no spine room: also connect left↔right so they're
        // both reachable even if the chaining only grabbed left as anchor
        if (slots[i].spine_id < 0 &&
            slots[i].left_id  >= 0 &&
            slots[i].right_id >= 0 &&
            slots[i].left_id  != anch)
        {
            corridors.push_back(make_corridor(
                slots[i].left_id, slots[i].right_id,
                rooms[slots[i].left_id], rooms[slots[i].right_id],
                MAIN_HW, false));
        }
    }

    // (c) Shuttle bay — connect to the last engine room (stern anchor)
    {
        int last_anch = anchor[spine_n-1];
        if (last_anch >= 0) {
            corridors.push_back(make_corridor(
                last_anch, shuttle_room_id,
                rooms[last_anch], rooms[shuttle_room_id],
                MAIN_HW, false));
        }
    }

    // (d) Extra cross-connections — 1–2 short connections to add cycles
    //     (prevents pure tree layout; gives crew alternate escape routes)
    {
        // Build connected set for quick lookup
        std::set<std::pair<int,int>> connected;
        for (const auto& c : corridors)
            connected.insert({std::min(c.room_a, c.room_b),
                               std::max(c.room_a, c.room_b)});

        struct Cand { int a, b; double dist; };
        std::vector<Cand> cands;
        for (int a = 0; a < (int)rooms.size(); ++a)
            for (int b = a+1; b < (int)rooms.size(); ++b) {
                if (connected.count({a, b})) continue;
                double d = std::hypot(rooms[a].cx - rooms[b].cx,
                                      rooms[a].cy - rooms[b].cy);
                if (d < 70.0 * scale) cands.push_back({a, b, d});
            }
        std::sort(cands.begin(), cands.end(),
                  [](const Cand& x, const Cand& y){ return x.dist < y.dist; });

        int added = 0;
        for (auto& cand : cands) {
            if (added >= 2) break;
            if (connected.count({cand.a, cand.b})) continue;
            corridors.push_back(make_corridor(
                cand.a, cand.b,
                rooms[cand.a], rooms[cand.b],
                MAIN_HW, false));
            connected.insert({cand.a, cand.b});
            ++added;
        }
    }

    // ── Stage 7: Vent shafts ──────────────────────────────────────────────────
    // (a) Mark dead-end rooms (degree 1) as VentShaft — feels isolated and hidden
    {
        std::vector<int> degree((int)rooms.size(), 0);
        for (const auto& c : corridors) { degree[c.room_a]++; degree[c.room_b]++; }

        std::vector<int> dead_ends;
        for (int i = 0; i < (int)rooms.size(); ++i) {
            if (degree[i] == 1 &&
                rooms[i].type != RoomType::Bridge    &&
                rooms[i].type != RoomType::ShuttleBay &&
                !rooms[i].is_engine())
                dead_ends.push_back(i);
        }
        std::shuffle(dead_ends.begin(), dead_ends.end(), rng);
        int vent_n = std::min((int)dead_ends.size(), 2);
        for (int i = 0; i < vent_n; ++i) {
            rooms[dead_ends[i]].type = RoomType::VentShaft;
            vent_room_ids.push_back(dead_ends[i]);
        }
    }

    // (b) Add narrow vent-shaft corridors between nearby non-adjacent rooms
    {
        std::set<std::pair<int,int>> connected;
        for (const auto& c : corridors)
            connected.insert({std::min(c.room_a, c.room_b),
                               std::max(c.room_a, c.room_b)});

        struct Cand { int a, b; double dist; };
        std::vector<Cand> cands;
        for (int a = 0; a < (int)rooms.size(); ++a)
            for (int b = a+1; b < (int)rooms.size(); ++b) {
                if (connected.count({a, b})) continue;
                double d = std::hypot(rooms[a].cx - rooms[b].cx,
                                      rooms[a].cy - rooms[b].cy);
                if (d < 50.0 * scale) cands.push_back({a, b, d});
            }
        std::sort(cands.begin(), cands.end(),
                  [](const Cand& x, const Cand& y){ return x.dist < y.dist; });

        int added = 0;
        for (auto& cand : cands) {
            if (added >= 3) break;
            corridors.push_back(make_corridor(
                cand.a, cand.b,
                rooms[cand.a], rooms[cand.b],
                VENT_HW, false, true));  // narrow, is_vent_shaft = true
            ++added;
        }
    }

    // ── Stage 8: Hull polygon ─────────────────────────────────────────────────
    // Collect expanded corners of all rooms, compute convex hull, then taper
    // the nose and stern tips toward the spine centre line.
    {
        constexpr double PAD = 9.0;
        std::vector<std::array<double,2>> pts;
        pts.reserve(rooms.size() * 4);
        for (const auto& r : rooms) {
            pts.push_back({r.cx - r.hw - PAD, r.cy - r.hh - PAD});
            pts.push_back({r.cx + r.hw + PAD, r.cy - r.hh - PAD});
            pts.push_back({r.cx + r.hw + PAD, r.cy + r.hh + PAD});
            pts.push_back({r.cx - r.hw - PAD, r.cy + r.hh + PAD});
        }

        auto hull = convex_hull(pts);

        // Post-process: squeeze nose (high Y) and stern (low Y) toward x=0
        double nose_thresh  = nose_y  - profile.spine_length * 0.08;
        double stern_thresh = stern_y + profile.spine_length * 0.10;
        for (auto& p : hull) {
            if (p[1] > nose_thresh) {
                double frac = (p[1] - nose_thresh) / (nose_y + PAD - nose_thresh + 1e-9);
                frac = std::clamp(frac, 0.0, 1.0);
                p[0] *= (1.0 - frac);
            }
            if (p[1] < stern_thresh) {
                double frac = (stern_thresh - p[1]) / (stern_thresh - stern_y + PAD + 1e-9);
                frac = std::clamp(frac, 0.0, 1.0);
                p[0] *= (1.0 - frac * 0.75); // stern narrows but stays wider than nose
            }
        }

        for (const auto& p : hull) {
            hull_xs.push_back(p[0]);
            hull_ys.push_back(p[1]);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// Walkability & movement (unchanged logic, updated to call is_vent() as method)
// ═════════════════════════════════════════════════════════════════════════════
bool Ship::is_walkable(double x, double y) const noexcept {
    for (const auto& r : rooms)     if (r.contains(x, y)) return true;
    for (const auto& c : corridors) if (c.contains(x, y)) return true;
    return false;
}

std::array<double,2> Ship::resolve_movement(
    const std::array<double,2>& old_pos,
    const std::array<double,2>& new_pos) const noexcept
{
    if (is_walkable(new_pos[0], new_pos[1])) return new_pos;
    if (is_walkable(new_pos[0], old_pos[1])) return {new_pos[0], old_pos[1]};
    if (is_walkable(old_pos[0], new_pos[1])) return {old_pos[0], new_pos[1]};
    return old_pos;
}

// ═════════════════════════════════════════════════════════════════════════════
// Spatial queries (unchanged)
// ═════════════════════════════════════════════════════════════════════════════
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

// ═════════════════════════════════════════════════════════════════════════════
// BFS pathfinding (unchanged — still works on the new graph)
// ═════════════════════════════════════════════════════════════════════════════
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

    if (prev[to_id] < 0) return {room_center(to_id)};  // no path — go direct

    std::vector<int> path;
    for (int cur = to_id; cur != from_id; cur = prev[cur]) path.push_back(cur);
    path.push_back(from_id);
    std::reverse(path.begin(), path.end());

    std::vector<std::array<double,2>> wps;
    for (int i = 0; i+1 < (int)path.size(); ++i) {
        int ci = corr_used[path[i+1]];
        if (ci >= 0) {
            const auto& c = corridors[ci];
            if (c.room_a == path[i]) wps.push_back({c.bx, c.by});
            else                     wps.push_back({c.x2, c.y1});
        }
        wps.push_back(room_center(path[i+1]));
    }
    return wps;
}