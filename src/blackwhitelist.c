/**
 * @file blackwhitelist.c
 * @brief Domain hash-table for blacklist / allowlist filtering.
 *
 * Loads domain names from a text file into a uthash hash table and
 * provides O(1) hostname lookup with subdomain matching.
 *
 * Portions of this file are derived from GoodbyeDPI by ValdikSS.
 * GoodbyeDPI: https://github.com/ValdikSS/GoodbyeDPI
 * Copyright (C) 2018 ValdikSS, licensed under the Apache License 2.0.
 * Modifications copyright (C) 2024-2026 Gokhan Ozen (gokhazen).
 * See NOTICE and licenses/LICENSE-goodbyedpi.txt for full terms.
 *
 * SPDX-License-Identifier: MIT AND Apache-2.0
 */
#include <windows.h>
#include <stdio.h>
#include "hayalet.h"
#include "utils/uthash.h"
#include "utils/getline.h"

typedef struct blackwhitelist_record {
    const char *host;
    UT_hash_handle hh;   /* makes this structure hashable */
} blackwhitelist_record_t;

static blackwhitelist_record_t *blackwhitelist = NULL;

static int check_get_hostname(const char *host) {
    blackwhitelist_record_t *tmp_record = NULL;
    if (!blackwhitelist) return FALSE;

    HASH_FIND_STR(blackwhitelist, host, tmp_record);
    if (tmp_record) {
        debug("check_get_hostname found host\n");
        return TRUE;
    }
    debug("check_get_hostname host not found\n");
    return FALSE;
}

static int add_hostname(const char *host) {
    if (!host)
        return FALSE;

    blackwhitelist_record_t *tmp_record = malloc(sizeof(blackwhitelist_record_t));
    char *host_c = NULL;

    if (!check_get_hostname(host)) {
        host_c = strdup(host);
        tmp_record->host = host_c;
        HASH_ADD_KEYPTR(hh, blackwhitelist, tmp_record->host,
                        strlen(tmp_record->host), tmp_record);
        debug("Added host %s\n", host_c);
        return TRUE;
    }
    debug("Not added host %s\n", host);
    free(tmp_record);
    if (host_c)
        free(host_c);
    return FALSE;
}

int blackwhitelist_load_list(const char *filename) {
    char *line = malloc(HOST_MAXLEN + 1);
    size_t linelen = HOST_MAXLEN + 1;
    int cnt = 0;
    ssize_t read;

    FILE *fp = fopen(filename, "r");
    if (!fp) return FALSE;

    while ((read = getline(&line, &linelen, fp)) != -1) {
        /* works with both \n and \r\n */
        line[strcspn(line, "\r\n")] = '\0';
        if (strlen(line) > HOST_MAXLEN) {
            printf("WARNING: host %s exceeds maximum host length and has not been added\n",
                line);
            continue;
        }
        if (strlen(line) < 2) {
            printf("WARNING: host %s is less than 2 characters, skipping\n", line);
            continue;
        }
        if (add_hostname(line))
            cnt++;
    }
    free(line);
    if (!blackwhitelist) return FALSE;
    printf("Loaded %d hosts from file %s\n", cnt, filename);
    fclose(fp);
    return TRUE;
}

int blackwhitelist_check_hostname(const char *host_addr, size_t host_len) {
    char current_host[HOST_MAXLEN + 1];
    char *tokenized_host = NULL;

    if (host_len > HOST_MAXLEN) return FALSE;
    if (host_addr && host_len) {
        memcpy(current_host, host_addr, host_len);
        current_host[host_len] = '\0';
    }

    if (check_get_hostname(current_host))
            return TRUE;

    tokenized_host = strchr(current_host, '.');
    while (tokenized_host != NULL && tokenized_host < (current_host + HOST_MAXLEN)) {
        if (check_get_hostname(tokenized_host + 1))
            return TRUE;
        tokenized_host = strchr(tokenized_host + 1, '.');
    }

    debug("____blackwhitelist_check_hostname FALSE: host %s\n", current_host);
    return FALSE;
}

void blackwhitelist_clear() {
    blackwhitelist_record_t *current_record, *tmp;
    HASH_ITER(hh, blackwhitelist, current_record, tmp) {
        HASH_DEL(blackwhitelist, current_record);
        if (current_record->host) free((char*)current_record->host);
        free(current_record);
    }
    blackwhitelist = NULL;
}
