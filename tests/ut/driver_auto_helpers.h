#ifndef NEU_DRIVER_AUTO_TEST_HELPERS_H
#define NEU_DRIVER_AUTO_TEST_HELPERS_H

#include "type.h"

int  is_value_in_range(neu_type_e, int64_t, double, neu_type_e, double);
int  auto_test_prepare(neu_type_e, neu_dvalue_t *, double, double, const char *,
                       int, bool *, neu_type_e *);
void auto_test_read(neu_dvalue_t, double, const char *, int, neu_dvalue_t *,
                    neu_type_e *, neu_type_e *);
int  auto_test_metadata(neu_type_e, bool, int *, neu_type_e *);

typedef struct {
    neu_type_e   configured_type;
    neu_type_e   learned_type;
    neu_dvalue_t sample;
    bool         missing_cache;
    bool         pending;
    bool         expired;
    bool         write_only;
    int          error;
} auto_test_snapshot_case_t;

typedef struct {
    neu_dvalue_t value;
    neu_type_e   configured_type;
    neu_type_e   listed_type;
    neu_type_e   read_type;
    unsigned int read_count;
} auto_test_snapshot_result_t;

void auto_test_snapshot(const auto_test_snapshot_case_t *, bool,
                        auto_test_snapshot_result_t *);

#endif
