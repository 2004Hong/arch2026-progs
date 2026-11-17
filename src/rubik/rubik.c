#include "rubik.h"
#include <string.h>
#include "rubik_five_step.h"
#include "rubik_tables.h"

/* Use precomputed transition and distance tables.
 * Complete search suffixes through the five-move table whenever
 * the remaining depth budget is at most five.
 */
enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    MOVES = 9,
    MAX_DEPTH = 11
};
typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;
typedef struct {
    uint64_t expanded, generated;
} search_stats_t;

static uint32_t rank_state(const state_t *state)
{
    uint32_t p = 0, o = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (CUBIES - i) + smaller;
    }
    for (uint8_t i = 0; i < 6; ++i)
        o = o * 3U + state->o[i];
    return p * ORIENTATIONS + o;
}

static int valid(const state_t *state)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;
        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;
        sum = (uint8_t) (sum + state->o[i]);
    }
    return sum % 3U == 0;
}

static uint8_t heuristic(uint16_t p, uint16_t o)
{
    uint8_t hp = permutation_distance[p];
    uint8_t ho = orientation_distance[o];

    return hp > ho ? hp : ho;
}

/* Declare the table solver before it is used by the search. */
static int solve_five_step(uint16_t p,
                           uint16_t o,
                           unsigned remaining,
                           uint8_t *path,
                           unsigned *length);

static int search_with_limit(uint16_t start_p,
                             uint16_t start_o,
                             unsigned limit,
                             uint8_t path[MAX_DEPTH],
                             unsigned *length,
                             search_stats_t *stats)
{
    uint16_t p_states[MAX_DEPTH + 1];
    uint16_t o_states[MAX_DEPTH + 1];
    uint16_t next_p_cache[MAX_DEPTH + 1];
    uint16_t next_o_cache[MAX_DEPTH + 1];
    uint8_t next_move[MAX_DEPTH + 1] = {0};
    unsigned depth = 0;
    *length = 0;
    if (limit > MAX_DEPTH)
        return 0;
    p_states[0] = start_p;
    o_states[0] = start_o;

    for (;;) {
        if (p_states[depth] == 0 && o_states[depth] == 0) {
            *length = depth;
            return 1;
        }

        if (depth == limit || next_move[depth] == MOVES ||
            depth + heuristic(p_states[depth], o_states[depth]) > limit) {
            if (depth == 0)
                return 0;
            --depth;
            continue;
        }

        /* Use the table instead of expanding the final five search levels. */
        unsigned remaining = limit - depth;

        if (remaining <= 5) {
            unsigned suffix_length = 0;

            if (solve_five_step(p_states[depth], o_states[depth], remaining,
                                path + depth, &suffix_length)) {
                /* Append the table solution after the searched prefix. */
                *length = depth + suffix_length;
                return 1;
            }

            /* No solution fits the remaining budget; backtrack immediately. */
            if (depth == 0)
                return 0;

            --depth;
            continue;
        }

        if (next_move[depth] == 0)
            ++stats->expanded;
        uint8_t move = next_move[depth]++;

        if (depth > 0 && move / 3U == path[depth - 1] / 3U)
            continue;

        uint8_t face = (uint8_t) (move / 3U);

        if (move % 3U == 0) {
            next_p_cache[depth] = p_states[depth];
            next_o_cache[depth] = o_states[depth];
        }
        next_p_cache[depth] = permutation_next[face][next_p_cache[depth]];
        next_o_cache[depth] = orientation_next[face][next_o_cache[depth]];

        p_states[depth + 1] = next_p_cache[depth];
        o_states[depth + 1] = next_o_cache[depth];

        path[depth] = move;
        ++stats->generated;
        ++depth;
        next_move[depth] = 0;
    }
}

static int solve_ida(const state_t *start,
                     unsigned max_depth,
                     uint8_t path[MAX_DEPTH],
                     unsigned *length,
                     search_stats_t *total,
                     int verbose)
{
    (void) verbose;
    uint32_t rank = rank_state(start);
    uint16_t start_p = (uint16_t) (rank / ORIENTATIONS);
    uint16_t start_o = (uint16_t) (rank % ORIENTATIONS);

    total->expanded = 0;
    total->generated = 0;
    if (max_depth > MAX_DEPTH)
        return 0;
    for (unsigned limit = heuristic(start_p, start_o); limit <= max_depth;
         ++limit) {
        search_stats_t current = {0, 0};
        int found =
            search_with_limit(start_p, start_o, limit, path, length, &current);
        total->expanded += current.expanded;
        total->generated += current.generated;
        if (found)
            return 1;
    }
    return 0;
}

static int parse_state(const char *input, state_t *state)
{
    for (int i = 0; i < 14; ++i) {
        int limit = i < 7 ? 7 : 3;
        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;
        (i < 7 ? state->p : state->o)[i % 7] = (uint8_t) (input[i] - '1');
    }
    return input[14] == '\0' && valid(state);
}

/* Use binary search to find the current rank in the five-move table. */
static int find_five_step(uint32_t rank)
{
    unsigned left = 0;
    unsigned right = RUBIK_FIVE_STEP_COUNT;

    while (left < right) {
        unsigned mid = left + (right - left) / 2;

        if (five_step_ranks[mid] < rank)
            left = mid + 1;
        else
            right = mid;
    }

    if (left < RUBIK_FIVE_STEP_COUNT && five_step_ranks[left] == rank)
        return (int) left;

    return -1;
}

/* Follow table moves from the supplied coordinates within the given budget. */
static int solve_five_step(uint16_t p,
                           uint16_t o,
                           unsigned remaining,
                           uint8_t *path,
                           unsigned *length)
{
    unsigned steps = 0;

    while (p != 0 || o != 0) {
        int index = find_five_step((uint32_t) p * ORIENTATIONS + o);

        /* Reject missing states or paths exceeding either move limit. */
        if (index < 0 || steps >= 5 || steps >= remaining)
            return 0;

        uint8_t move = five_step_next_move[index];
        if (move >= MOVES)
            return 0;

        path[steps++] = move;

        /* Our order is R, R2, R', B, B2, B', D, D2, D'. */
        unsigned face = move / 3U;
        unsigned turns = move % 3U + 1U;

        /* The existing transition tables describe quarter turns. */
        for (unsigned i = 0; i < turns; ++i) {
            p = permutation_next[face][p];
            o = orientation_next[face][o];
        }
    }

    *length = steps;
    return 1;
}

/* Official API: validate before writing anything to the caller's buffer. */
int rubik_solve(const char *input, char *out, size_t cap)
{
    static const char *const names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                             "B'", "D",  "D2", "D'"};
    state_t state;
    uint8_t path[MAX_DEPTH];
    unsigned length = 0;
    search_stats_t stats;
    if (!parse_state(input, &state))
        return -1;
    if (cap == 0)
        return -2;
    uint32_t rank = rank_state(&state);
    if (rank == 0) {
        out[0] = '\0';
        return 0;
    }

    /* Try a direct five-move solution before starting IDA* search. */
    if (!solve_five_step((uint16_t) (rank / ORIENTATIONS),
                         (uint16_t) (rank % ORIENTATIONS), 5, path, &length) &&
        !solve_ida(&state, MAX_DEPTH, path, &length, &stats, 0))
        return -1;
    size_t need = 1;
    for (unsigned i = 0; i < length; ++i) {
        if (i != 0)
            ++need;
        const char *p = names[path[i]];
        while (*p++)
            ++need;
    }
    if (cap < need) {
        out[0] = '\0';
        return -2;
    }
    char *dst = out;
    for (unsigned i = 0; i < length; ++i) {
        if (i != 0)
            *dst++ = ' ';
        const char *p = names[path[i]];
        while (*p)
            *dst++ = *p++;
    }
    *dst = '\0';
    return (int) length;
}
