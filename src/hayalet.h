#include <stdint.h>
#define HOST_MAXLEN 253
#define MAX_PACKET_SIZE 9016

#ifndef DEBUG
#define debug(...) do {} while (0)
#else
#define debug(...) printf(__VA_ARGS__)
#endif

extern uint64_t packets_processed;
extern uint64_t packets_circumvented;

int run_hayalet(int argc, char *argv[]);
void deinit_all();
void shutdown_hayalet();
void stop_hayalet();
void reset_hayalet_state();
