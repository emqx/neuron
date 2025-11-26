#ifndef NEU_PLUGIN_REST_SIMULATOR_HANDLE_H
#define NEU_PLUGIN_REST_SIMULATOR_HANDLE_H

#include <nng/nng.h>

void handle_simulator_status(nng_aio *aio);
void handle_simulator_start(nng_aio *aio);
void handle_simulator_stop(nng_aio *aio);
void handle_simulator_set_config(nng_aio *aio);
void handle_simulator_export(nng_aio *aio);
void handle_simulator_list_tags(nng_aio *aio);

#endif