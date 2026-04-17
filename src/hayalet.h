/**
 * @file hayalet.h
 * @brief HayaletDPI shared types, constants, and engine API declarations.
 *
 * Central header included by all translation units. Defines:
 *  - Path constants for user-configuration files
 *  - HayaletConfig: serialisable engine configuration struct
 *  - Hayalet_* engine lifecycle API (RunConfig, Stop, Reset)
 *  - Packet counters exported from hayalet.c for real-time telemetry
 *
 * @copyright Copyright (C) 2024-2026 Gokhan Ozen (gokhazen)
 * @license MIT — see LICENSE in the repository root
 * SPDX-License-Identifier: MIT
 */
#define USER_FILES_DIR "userfiles"
#define USER_FILES_PATH USER_FILES_DIR "/"
#include "version.h"

#include <stdint.h>
#define HOST_MAXLEN 253
#define MAX_PACKET_SIZE 9016

#ifndef DEBUG
#define debug(...) do {} while (0)
#else
#define debug(...) printf(__VA_ARGS__)
#endif
#include <stdint.h>
#include <windows.h>

extern uint64_t packets_processed;
extern uint64_t packets_circumvented;

typedef struct {
    int cfg_mode;           // 0 to -9
    int cfg_filter_mode;    // 0=All, 1=Blacklist, 2=SelectedApps, 3=Gaming
    char cfg_dns_addr[64];
    int cfg_dns_port;
    char cfg_dnsv6_addr[64];
    int cfg_dnsv6_port;
    char cfg_blacklist_path[MAX_PATH];
    int cfg_use_doh;
    char cfg_custom_args[512];
    int cfg_allowlist_enabled;
} HayaletConfig;

// Core Engine API
int run_hayalet(int argc, char *argv[]); // Legacy bridge
int Hayalet_RunConfig(HayaletConfig config); 
void Hayalet_Stop();
void Hayalet_Reset();

// Support functions
void deinit_all();
void shutdown_hayalet();
void reset_hayalet_state();
void stop_hayalet();
