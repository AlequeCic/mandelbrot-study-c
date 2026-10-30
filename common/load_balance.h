#ifndef LOAD_BALANCE_H
#define LOAD_BALANCE_H

/*
 * Classroom FBC definition based only on computation times:
 *     (T_max - T_min) / T_max
 * Zero means perfect balance; values closer to one mean strong imbalance.
 */
static inline double calculate_load_balance_factor(double min_seconds,
                                                   double max_seconds) {
    if (max_seconds <= 0.0 || min_seconds >= max_seconds) {
        return 0.0;
    }
    return (max_seconds - min_seconds) / max_seconds;
}

#endif
