#include "rubik.h"
#include <string.h>

/* Baseline IDA* solver using separate permutation and orientation ranks.
 * Build transition and distance tables on the first non-solved call.
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



static uint8_t permutation_distance[PERMUTATIONS];
static uint8_t orientation_distance[ORIENTATIONS];
static uint16_t permutation_next[3][PERMUTATIONS];
static uint16_t orientation_next[3][ORIENTATIONS];

static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};

static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

/* Encode the permutation with a Lehmer rank and the orientations in base 3.
 * The seventh orientation follows from the twist-sum constraint.
 */
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

static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1U < (unsigned) (CUBIES - i); ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
}

/* Build quarter-turn transitions separately for permutation and orientation. */
static void build_transitions(void)
{
    state_t state;

    for (uint16_t p = 0; p < PERMUTATIONS; ++p) {
        unrank_state((uint32_t) p * ORIENTATIONS, &state);

        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);

            permutation_next[face][p] =
                (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
    }

    for (uint16_t o = 0; o < ORIENTATIONS; ++o) {
        unrank_state(o, &state);

        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);

            orientation_next[face][o] =
                (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }
}

/* Use BFS from the solved coordinate to build minimum move distances.
 * Quarter, half and inverse turns each count as one move.
 */
static int build_permutation_distances(void)
{
    static uint16_t queue[PERMUTATIONS];
    unsigned head = 0;
    unsigned tail = 0;

    memset(permutation_distance, UINT8_MAX, sizeof permutation_distance);

    permutation_distance[0] = 0;
    queue[tail++] = 0;

    while (head < tail) {
        uint16_t here = queue[head++];

        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next = here;

            for (uint8_t turn = 0; turn < 3; ++turn) {
                next = permutation_next[face][next];

                if (next >= PERMUTATIONS) {
                    return 0;
                }

                if (permutation_distance[next] == UINT8_MAX) {
                    permutation_distance[next] =
                        (uint8_t) (permutation_distance[here] + 1);

                    queue[tail++] = next;
                }
            }
        }
    }

    if (tail != PERMUTATIONS) {
        return 0;
    }

    uint8_t maximum = 0;

    for (unsigned i = 0; i < PERMUTATIONS; ++i) {
        if (permutation_distance[i] > maximum)
            maximum = permutation_distance[i];
    }
    return 1;
}

/* Use BFS from the solved coordinate to build minimum move distances.
 * Quarter, half and inverse turns each count as one move.
 */
static int build_orientation_distances(void)
{
    static uint16_t queue[ORIENTATIONS];
    unsigned head = 0;
    unsigned tail = 0;

    memset(orientation_distance, UINT8_MAX, sizeof orientation_distance);

    orientation_distance[0] = 0;
    queue[tail++] = 0;

    while (head < tail) {
        uint16_t here = queue[head++];

        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next = here;

            for (uint8_t turn = 0; turn < 3; ++turn) {
                next = orientation_next[face][next];

                if (next >= ORIENTATIONS) {
                    return 0;
                }

                if (orientation_distance[next] == UINT8_MAX) {
                    orientation_distance[next] =
                        (uint8_t) (orientation_distance[here] + 1);

                    queue[tail++] = next;
                }
            }
        }
    }

    if (tail != ORIENTATIONS) {
        return 0;
    }

    uint8_t maximum = 0;

    for (unsigned i = 0; i < ORIENTATIONS; ++i) {
        if (orientation_distance[i] > maximum)
            maximum = orientation_distance[i];
    }

    return 1;
}

/* Require a valid permutation and an orientation sum divisible by three. */
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

/* The larger coordinate distance is a lower bound on the full solution.
 * Do not add them: one move can improve both coordinates.
 */
static uint8_t heuristic(uint16_t p, uint16_t o)
{
    uint8_t hp = permutation_distance[p];
    uint8_t ho = orientation_distance[o];

    return hp > ho ? hp : ho;
}


static int search_with_limit(uint16_t start_p,
                             uint16_t start_o,
                             unsigned limit,
                             uint8_t path[MAX_DEPTH],
                             unsigned *length,
                             search_stats_t *stats)
{
    /* Store search levels explicitly instead of using recursive calls. */
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
        /* Backtrack when the limit is reached, all moves are tried,
         * or depth plus the lower bound exceeds the current limit.
         */
        if (depth == limit || next_move[depth] == MOVES ||
            depth + heuristic(p_states[depth], o_states[depth]) > limit) {
            if (depth == 0)
                return 0;
            --depth;
            continue;
        }

        if (next_move[depth] == 0)
            ++stats->expanded;
        uint8_t move = next_move[depth]++;

        /* Consecutive turns of the same face can be combined into a shorter
         * path. */
        if (depth > 0 && move / 3U == path[depth - 1] / 3U)
            continue;

        uint8_t face = (uint8_t) (move / 3U);

        /* Reuse quarter-turn results to generate half and inverse turns. */
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

/* Increase the depth limit from the lower bound.
 * The first successful limit gives a shortest solution.
 */
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


/* Official API: validate before writing anything to the caller's buffer. */
int rubik_solve(const char *input, char *out, size_t cap)
{
    static int tables_ready;
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
    if (!tables_ready) {
        build_transitions();
        if (!build_permutation_distances() || !build_orientation_distances())
            return -1;
        tables_ready = 1;
    }
    if (!solve_ida(&state, MAX_DEPTH, path, &length, &stats, 0))
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
