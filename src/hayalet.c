/*
 * GoodbyeDPI — Passive DPI blocker and Active DPI circumvention utility.
 */

#include "hayalet.h"
#include "blackwhitelist.h"
#include "dnsredir.h"
#include "fakepackets.h"
#include "service.h"
#include "ttltrack.h"
#include "utils/repl_str.h"
#include "windivert.h"
#include <ctype.h>
#include <getopt.h>
#include <in6addr.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tcpestats.h>
#include <time.h>
#include <unistd.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <direct.h>

// My mingw installation does not load inet_pton definition for some reason
WINSOCK_API_LINKAGE INT WSAAPI inet_pton(INT Family, LPCSTR pStringBuf,
                                         PVOID pAddr);

#include "version.h"

void GetProcessNameByPid(DWORD pid, char *outName, DWORD outSize);

#define die()                                                                  \
  do {                                                                         \
    sleep(20);                                                                 \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define MAX_FILTERS 4

#define DIVERT_NO_LOCALNETSv4_DST                                              \
  "("                                                                          \
  "(ip.DstAddr < 127.0.0.1 or ip.DstAddr > 127.255.255.255) and "              \
  "(ip.DstAddr < 10.0.0.0 or ip.DstAddr > 10.255.255.255) and "                \
  "(ip.DstAddr < 192.168.0.0 or ip.DstAddr > 192.168.255.255) and "            \
  "(ip.DstAddr < 172.16.0.0 or ip.DstAddr > 172.31.255.255) and "              \
  "(ip.DstAddr < 169.254.0.0 or ip.DstAddr > 169.254.255.255)"                 \
  ")"
#define DIVERT_NO_LOCALNETSv4_SRC                                              \
  "("                                                                          \
  "(ip.SrcAddr < 127.0.0.1 or ip.SrcAddr > 127.255.255.255) and "              \
  "(ip.SrcAddr < 10.0.0.0 or ip.SrcAddr > 10.255.255.255) and "                \
  "(ip.SrcAddr < 192.168.0.0 or ip.SrcAddr > 192.168.255.255) and "            \
  "(ip.SrcAddr < 172.16.0.0 or ip.SrcAddr > 172.31.255.255) and "              \
  "(ip.SrcAddr < 169.254.0.0 or ip.SrcAddr > 169.254.255.255)"                 \
  ")"

#define DIVERT_NO_LOCALNETSv6_DST                                              \
  "("                                                                          \
  "(ipv6.DstAddr > ::1) and "                                                  \
  "(ipv6.DstAddr < 2001::0 or ipv6.DstAddr > 2001:1::0) and "                  \
  "(ipv6.DstAddr < fc00::0 or ipv6.DstAddr > fe00::0) and "                    \
  "(ipv6.DstAddr < fe80::0 or ipv6.DstAddr > fec0::0) and "                    \
  "(ipv6.DstAddr < ff00::0 or ipv6.DstAddr > ffff::0)"                         \
  ")"
#define DIVERT_NO_LOCALNETSv6_SRC                                              \
  "("                                                                          \
  "(ipv6.SrcAddr > ::1) and "                                                  \
  "(ipv6.SrcAddr < 2001::0 or ipv6.SrcAddr > 2001:1::0) and "                  \
  "(ipv6.SrcAddr < fc00::0 or ipv6.SrcAddr > fe00::0) and "                    \
  "(ipv6.SrcAddr < fe80::0 or ipv6.SrcAddr > fec0::0) and "                    \
  "(ipv6.SrcAddr < ff00::0 or ipv6.SrcAddr > ffff::0)"                         \
  ")"

/* #IPID# is a template to find&replace */
#define IPID_TEMPLATE "#IPID#"
#define MAXPAYLOADSIZE_TEMPLATE "#MAXPAYLOADSIZE#"

enum ERROR_CODE {
  ERROR_DEFAULT = 1,
  ERROR_PORT_BOUNDS,
  ERROR_DNS_V4_ADDR,
  ERROR_DNS_V6_ADDR,
  ERROR_DNS_V4_PORT,
  ERROR_DNS_V6_PORT,
  ERROR_BLACKLIST_LOAD,
  ERROR_AUTOTTL,
  ERROR_ATOUSI,
  ERROR_AUTOB
};

static int running_from_service = 0;
static int exiting = 0;
int active_filter_mode = 0; // 0=All, 1=Blacklist, 2=SelectedApps
uint64_t packets_processed = 0;
uint64_t packets_circumvented = 0;
static HANDLE filters[MAX_FILTERS];
static int filter_num = 0;

// Global Port-to-PID tracking for Select Apps mode
typedef struct {
  uint32_t pid;
} port_info_t;
port_info_t tcp_port_map[65536] = {0};
port_info_t udp_port_map[65536] = {0};

char filter_apps[1024][MAX_PATH];
int filter_apps_count = 0;
time_t last_apps_load_time = 0;

// ---- Allowlist (DPI bypass bypass) ----
#define ALLOWLIST_MAX 512
char allowlist_domains[ALLOWLIST_MAX][256];
int  allowlist_count = 0;
int  allowlist_enabled = 0; // read from INI at startup
time_t last_allowlist_load_time = 0;

void LoadAllowlist() {
  time_t now = time(NULL);
  if (now - last_allowlist_load_time < 5 && allowlist_count >= 0) return;
  allowlist_count = 0;
  FILE *f = fopen(USER_FILES_PATH "allowlist.txt", "r");
  if (f) {
    char line[256];
    while (fgets(line, sizeof(line), f) && allowlist_count < ALLOWLIST_MAX) {
      size_t len = strlen(line);
      while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) { line[len-1] = '\0'; len--; }
      if (len > 0) strncpy(allowlist_domains[allowlist_count++], line, 255);
    }
    fclose(f);
    last_allowlist_load_time = now;
  }
}

// ---- Engine State & Flags (Global Scope) ----
static struct {
    int do_passivedpi, do_block_quic, do_fragment_http,
        do_fragment_http_persistent, do_fragment_http_persistent_nowait,
        do_fragment_https, do_host, do_host_removespace,
        do_additional_space, do_http_allports, do_host_mixedcase,
        do_dnsv4_redirect, do_dnsv6_redirect, do_dns_verb,
        do_tcp_verb, do_blacklist, do_allow_no_sni,
        do_fragment_by_sni, do_fake_packet, do_auto_ttl,
        do_wrong_chksum, do_wrong_seq, do_native_frag,
        do_reverse_frag;
    unsigned int http_fragment_size;
    unsigned int https_fragment_size;
    unsigned int current_fragment_size;
    unsigned short max_payload_size;
    BYTE should_send_fake;
    BYTE ttl_of_fake_packet;
    BYTE ttl_min_nhops;
    BYTE auto_ttl_1;
    BYTE auto_ttl_2;
    BYTE auto_ttl_max;
    uint32_t dnsv4_addr;
    struct in6_addr dnsv6_addr;
    struct in6_addr dns_temp_addr;
    uint16_t dnsv4_port;
    uint16_t dnsv6_port;
    char *host_addr, *useragent_addr, *method_addr;
    unsigned int host_len, useragent_len;
    int http_req_fragmented;
    HANDLE w_filter;
    bool debug_exit;
} engine = {0};

// Redirect old local variable names to the global engine struct
#define do_passivedpi engine.do_passivedpi
#define do_block_quic engine.do_block_quic
#define do_fragment_http engine.do_fragment_http
#define do_fragment_http_persistent engine.do_fragment_http_persistent
#define do_fragment_http_persistent_nowait engine.do_fragment_http_persistent_nowait
#define do_fragment_https engine.do_fragment_https
#define do_host engine.do_host
#define do_host_removespace engine.do_host_removespace
#define do_additional_space engine.do_additional_space
#define do_http_allports engine.do_http_allports
#define do_host_mixedcase engine.do_host_mixedcase
#define do_dnsv4_redirect engine.do_dnsv4_redirect
#define do_dnsv6_redirect engine.do_dnsv6_redirect
#define do_dns_verb engine.do_dns_verb
#define do_tcp_verb engine.do_tcp_verb
#define do_blacklist engine.do_blacklist
#define do_allow_no_sni engine.do_allow_no_sni
#define do_fragment_by_sni engine.do_fragment_by_sni
#define do_fake_packet engine.do_fake_packet
#define do_auto_ttl engine.do_auto_ttl
#define do_wrong_chksum engine.do_wrong_chksum
#define do_wrong_seq engine.do_wrong_seq
#define do_native_frag engine.do_native_frag
#define do_reverse_frag engine.do_reverse_frag
#define http_fragment_size engine.http_fragment_size
#define https_fragment_size engine.https_fragment_size
#define current_fragment_size engine.current_fragment_size
#define max_payload_size engine.max_payload_size
#define should_send_fake engine.should_send_fake
#define ttl_of_fake_packet engine.ttl_of_fake_packet
#define ttl_min_nhops engine.ttl_min_nhops
#define auto_ttl_1 engine.auto_ttl_1
#define auto_ttl_2 engine.auto_ttl_2
#define auto_ttl_max engine.auto_ttl_max
#define dnsv4_addr engine.dnsv4_addr
#define dnsv6_addr engine.dnsv6_addr
#define dns_temp_addr engine.dns_temp_addr
#define dnsv4_port engine.dnsv4_port
#define dnsv6_port engine.dnsv6_port
#define host_addr engine.host_addr
#define useragent_addr engine.useragent_addr
#define method_addr engine.method_addr
#define host_len engine.host_len
#define useragent_len engine.useragent_len
#define http_req_fragmented engine.http_req_fragmented
#define w_filter engine.w_filter
#define debug_exit engine.debug_exit

// Returns TRUE if hostname matches any entry in the allowlist (suffix match)
BOOL IsAllowlisted(const char *hostname, unsigned int hlen) {
  if (!allowlist_enabled || allowlist_count == 0) return FALSE;
  LoadAllowlist();
  char host[256];
  if (hlen >= sizeof(host)) return FALSE;
  memcpy(host, hostname, hlen);
  host[hlen] = '\0';
  for (int i = 0; i < allowlist_count; i++) {
    size_t alen = strlen(allowlist_domains[i]);
    if (alen == 0) continue;
    // exact or suffix match (e.g. "discord.com" matches "cdn.discord.com")
    if (_stricmp(host, allowlist_domains[i]) == 0) return TRUE;
    if (hlen > alen && host[hlen - alen - 1] == '.' &&
        _stricmp(host + hlen - alen, allowlist_domains[i]) == 0) return TRUE;
  }
  return FALSE;
}



// PID to Result cache to avoid constant OpenProcess calls
#define PID_CACHE_SIZE 256
typedef struct {
  DWORD pid;
  BOOL allowed;
  time_t timestamp;
} pid_cache_entry_t;
pid_cache_entry_t pid_cache[PID_CACHE_SIZE] = {0};
int pid_cache_ptr = 0;

// Function to lookup PID from port if flow monitor missed it
DWORD GetPidFromPort(uint16_t port, int protocol) {
  DWORD pid = 0;
  if (protocol == IPPROTO_TCP) {
    MIB_TCPTABLE_OWNER_PID *pTcpTable;
    DWORD dwSize = 0;
    if (GetExtendedTcpTable(NULL, &dwSize, FALSE, AF_INET,
                            TCP_TABLE_OWNER_PID_ALL,
                            0) == ERROR_INSUFFICIENT_BUFFER) {
      pTcpTable = (MIB_TCPTABLE_OWNER_PID *)malloc(dwSize);
      if (GetExtendedTcpTable(pTcpTable, &dwSize, FALSE, AF_INET,
                              TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
          if (ntohs((u_short)pTcpTable->table[i].dwLocalPort) == port) {
            pid = pTcpTable->table[i].dwOwningPid;
            break;
          }
        }
      }
      free(pTcpTable);
    }
  } else {
    MIB_UDPTABLE_OWNER_PID *pUdpTable;
    DWORD dwSize = 0;
    if (GetExtendedUdpTable(NULL, &dwSize, FALSE, AF_INET, UDP_TABLE_OWNER_PID,
                            0) == ERROR_INSUFFICIENT_BUFFER) {
      pUdpTable = (MIB_UDPTABLE_OWNER_PID *)malloc(dwSize);
      if (GetExtendedUdpTable(pUdpTable, &dwSize, FALSE, AF_INET,
                              UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
        for (DWORD i = 0; i < pUdpTable->dwNumEntries; i++) {
          if (ntohs((u_short)pUdpTable->table[i].dwLocalPort) == port) {
            pid = pUdpTable->table[i].dwOwningPid;
            break;
          }
        }
      }
      free(pUdpTable);
    }
  }
  return pid;
}

void LoadFilterApps() {
  time_t now = time(NULL);
  if (now - last_apps_load_time < 2 && filter_apps_count > 0)
    return; // Load every 2 seconds max

  last_apps_load_time = now;
  filter_apps_count = 0;
  FILE *f = fopen(USER_FILES_PATH "allowedapps.txt", "r");
  if (f) {
    char line[MAX_PATH];
    while (fgets(line, MAX_PATH, f)) {
      size_t len = strlen(line);
      while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
        line[len - 1] = '\0';
        len--;
      }
      if (len > 0) {
        strncpy(filter_apps[filter_apps_count++], line, MAX_PATH);
        if (filter_apps_count >= 1024)
          break;
      }
    }
    fclose(f);
    last_apps_load_time = now;
    // Invalidate PID cache when app list changes
    memset(pid_cache, 0, sizeof(pid_cache));
  }
}

BOOL IsProcessAllowed(DWORD pid) {
  if (active_filter_mode != 2 && active_filter_mode != 3)
    return TRUE;
  if (pid == 0)
    return FALSE;

  time_t now = time(NULL);
  // Check Cache
  for (int i = 0; i < PID_CACHE_SIZE; i++) {
    if (pid_cache[i].pid == pid && (now - pid_cache[i].timestamp < 60)) {
      return pid_cache[i].allowed;
    }
  }

  BOOL allowed = FALSE;
  if (active_filter_mode == 3) {
    char procName[MAX_PATH];
    GetProcessNameByPid(pid, procName, sizeof(procName));
    if (_stricmp(procName, "Discord.exe") == 0 ||
        _stricmp(procName, "DiscordCanary.exe") == 0 ||
        _stricmp(procName, "DiscordPTB.exe") == 0 ||
        _stricmp(procName, "RobloxPlayerBeta.exe") == 0 ||
        _stricmp(procName, "RobloxPlayerLauncher.exe") == 0 ||
        _stricmp(procName, "RobloxPlayer.exe") == 0 ||
        _stricmp(procName, "RobloxPlayerNormal.exe") == 0) {
      allowed = TRUE;
    }
  } else {
    HANDLE hProcess =
        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess) {
      char processPath[MAX_PATH];
      DWORD size = sizeof(processPath);
      if (QueryFullProcessImageName(hProcess, 0, processPath, &size)) {
        char *processName = strrchr(processPath, '\\');
        if (processName)
          processName++;
        else
          processName = processPath;

        for (int i = 0; i < filter_apps_count; i++) {
          if (_stricmp(processName, filter_apps[i]) == 0) {
            allowed = TRUE;
            break;
          }
        }
      }
      CloseHandle(hProcess);
    }
  }

  // Update Cache
  pid_cache[pid_cache_ptr].pid = pid;
  pid_cache[pid_cache_ptr].allowed = allowed;
  pid_cache[pid_cache_ptr].timestamp = now;
  pid_cache_ptr = (pid_cache_ptr + 1) % PID_CACHE_SIZE;

  return allowed;
}

void GetProcessNameByPid(DWORD pid, char *outName, DWORD outSize) {
  if (pid == 0) {
    strcpy(outName, "System/Unknown");
    return;
  }
  HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (hProcess) {
    char processPath[MAX_PATH];
    DWORD size = sizeof(processPath);
    if (QueryFullProcessImageName(hProcess, 0, processPath, &size)) {
      char *name = strrchr(processPath, '\\');
      if (name)
        strcpy(outName, name + 1);
      else
        strcpy(outName, processPath);
    } else {
      strcpy(outName, "Unknown");
    }
    CloseHandle(hProcess);
  } else {
    strcpy(outName, "Access Denied");
  }
}

void LogWithTime(const char *fmt, ...) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
  
  va_list args1, args2;
  va_start(args1, fmt);
  va_copy(args2, args1);
  vprintf(fmt, args1);
  va_end(args1);

  FILE *f = fopen(USER_FILES_PATH "hayalet.log", "a");
  if (f) {
    fprintf(f, "[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
    vfprintf(f, fmt, args2);
    fclose(f);
  }
  va_end(args2);
}

DWORD WINAPI flow_monitor_thread(LPVOID lpParam) {
  (void)lpParam;
  HANDLE hFlow = WinDivertOpen("true", WINDIVERT_LAYER_FLOW, 1000,
                               WINDIVERT_FLAG_SNIFF | WINDIVERT_FLAG_RECV_ONLY);
  if (hFlow == INVALID_HANDLE_VALUE)
    return 0;
  WINDIVERT_ADDRESS addr;
  while (!exiting) {
    if (WinDivertRecv(hFlow, NULL, 0, NULL, &addr)) {
      if (addr.Event == WINDIVERT_EVENT_FLOW_ESTABLISHED) {
        if (addr.Flow.Protocol == IPPROTO_TCP) {
          tcp_port_map[addr.Flow.LocalPort].pid = addr.Flow.ProcessId;
        } else if (addr.Flow.Protocol == IPPROTO_UDP) {
          udp_port_map[addr.Flow.LocalPort].pid = addr.Flow.ProcessId;
        }
      }
    }
  }
  WinDivertClose(hFlow);
  return 0;
}

#define FILTER_STRING_TEMPLATE                                                 \
  "(tcp and !impostor and !loopback " MAXPAYLOADSIZE_TEMPLATE " and "          \
  "((inbound and ("                                                            \
  "("                                                                          \
  "("                                                                          \
  "(ipv6 or (ip.Id >= 0x0 and ip.Id <= 0xF) " IPID_TEMPLATE ") and "           \
  "tcp.SrcPort == 80 and tcp.Ack"                                              \
  ") or "                                                                      \
  "((tcp.SrcPort == 80 or tcp.SrcPort == 443) and tcp.Ack and tcp.Syn)"        \
  ")"                                                                          \
  " and (" DIVERT_NO_LOCALNETSv4_SRC " or " DIVERT_NO_LOCALNETSv6_SRC          \
  "))) or "                                                                    \
  "(outbound and "                                                             \
  "(tcp.DstPort == 80 or tcp.DstPort == 443) and tcp.Ack and "                 \
  "(" DIVERT_NO_LOCALNETSv4_DST " or " DIVERT_NO_LOCALNETSv6_DST "))"          \
  "))"
#define FILTER_PASSIVE_BLOCK_QUIC                                              \
  "outbound and !impostor and !loopback and udp "                              \
  "and udp.DstPort == 443 and udp.PayloadLength >= 1200 "                      \
  "and udp.Payload[0] >= 0xC0 and udp.Payload32[1b] == 0x01"
#define FILTER_PASSIVE_STRING_TEMPLATE                                         \
  "inbound and ip and tcp and "                                                \
  "!impostor and !loopback and "                                               \
  "(true " IPID_TEMPLATE ") and "                                              \
  "(tcp.SrcPort == 443 or tcp.SrcPort == 80) and tcp.Rst "                     \
  "and " DIVERT_NO_LOCALNETSv4_SRC

#define SET_HTTP_FRAGMENT_SIZE_OPTION(fragment_size)                           \
  do {                                                                         \
    if (!http_fragment_size) {                                                 \
      http_fragment_size = (unsigned int)fragment_size;                        \
    } else if (http_fragment_size != (unsigned int)fragment_size) {            \
      printf(                                                                  \
          "WARNING: HTTP fragment size is already set to %u, not changing.\n", \
          http_fragment_size);                                                 \
    }                                                                          \
  } while (0)

#define TCP_HANDLE_OUTGOING_TTL_PARSE_PACKET_IF()                              \
  if ((packet_v4 && tcp_handle_outgoing(&ppIpHdr->SrcAddr, &ppIpHdr->DstAddr,  \
                                        ppTcpHdr->SrcPort, ppTcpHdr->DstPort,  \
                                        &tcp_conn_info, 0)) ||                 \
      (packet_v6 &&                                                            \
       tcp_handle_outgoing(ppIpV6Hdr->SrcAddr, ppIpV6Hdr->DstAddr,             \
                           ppTcpHdr->SrcPort, ppTcpHdr->DstPort,               \
                           &tcp_conn_info, 1)))

#define TCP_HANDLE_OUTGOING_FAKE_PACKET(func)                                  \
  do {                                                                         \
    should_send_fake = 1;                                                      \
    if (do_auto_ttl || ttl_min_nhops) {                                        \
      TCP_HANDLE_OUTGOING_TTL_PARSE_PACKET_IF() {                              \
        if (do_auto_ttl) {                                                     \
          /* If Auto TTL mode */                                               \
          ttl_of_fake_packet =                                                 \
              tcp_get_auto_ttl(tcp_conn_info.ttl, auto_ttl_1, auto_ttl_2,      \
                               ttl_min_nhops, auto_ttl_max);                   \
          if (do_tcp_verb) {                                                   \
            printf("Connection TTL = %d, Fake TTL = %d\n", tcp_conn_info.ttl,  \
                   ttl_of_fake_packet);                                        \
          }                                                                    \
        } else if (ttl_min_nhops) {                                            \
          /* If not Auto TTL mode but --min-ttl is set */                      \
          if (!tcp_get_auto_ttl(tcp_conn_info.ttl, 0, 0, ttl_min_nhops, 0)) {  \
            /* Send only if nhops >= min_ttl */                                \
            should_send_fake = 0;                                              \
          }                                                                    \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    if (should_send_fake) {                                                    \
      packets_circumvented++;                                                  \
      func(w_filter, &addr, packet, packetLen, packet_v6, ttl_of_fake_packet,  \
           do_wrong_chksum, do_wrong_seq);                                     \
    }                                                                          \
  } while (0)

static const char http10_redirect_302[] = "HTTP/1.0 302 ";
static const char http11_redirect_302[] = "HTTP/1.1 302 ";
static const char http_host_find[] = "\r\nHost: ";
static const char http_host_replace[] = "\r\nhoSt: ";
static const char http_useragent_find[] = "\r\nUser-Agent: ";
static const char location_http[] = "\r\nLocation: http://";
static const char connection_close[] = "\r\nConnection: close";
static const char *http_methods[] = {
    "GET ", "HEAD ", "POST ", "PUT ", "DELETE ", "CONNECT ", "OPTIONS ",
};
;

static struct option long_options[] = {
    {"port", required_argument, 0, 'z'},
    {"dns-addr", required_argument, 0, 'd'},
    {"dns-port", required_argument, 0, 'g'},
    {"dnsv6-addr", required_argument, 0, '!'},
    {"dnsv6-port", required_argument, 0, '@'},
    {"dns-verb", no_argument, 0, 'v'},
    {"blacklist", required_argument, 0, 'b'},
    {"allow-no-sni", no_argument, 0, ']'},
    {"frag-by-sni", no_argument, 0, '>'},
    {"ip-id", required_argument, 0, 'i'},
    {"set-ttl", required_argument, 0, '$'},
    {"min-ttl", required_argument, 0, '['},
    {"auto-ttl", optional_argument, 0, '+'},
    {"wrong-chksum", no_argument, 0, '%'},
    {"wrong-seq", no_argument, 0, ')'},
    {"native-frag", no_argument, 0, '*'},
    {"reverse-frag", no_argument, 0, '('},
    {"max-payload", optional_argument, 0, '|'},
    {"fake-from-hex", required_argument, 0, 'u'},
    {"fake-with-sni", required_argument, 0, '}'},
    {"fake-gen", required_argument, 0, 'j'},
    {"fake-resend", required_argument, 0, 't'},
    {"debug-exit", optional_argument, 0, 'x'},
    {"filter-mode", required_argument, 0, 'M'},
    {0, 0, 0, 0}};

static char *filter_string = NULL;
static char *filter_passive_string = NULL;

static void add_filter_str(int proto, int port) {
  const char *udp = " or (udp and !impostor and !loopback and "
                    "(udp.SrcPort == %d or udp.DstPort == %d))";
  const char *tcp =
      " or (tcp and !impostor and !loopback " MAXPAYLOADSIZE_TEMPLATE " and "
      "(tcp.SrcPort == %d or tcp.DstPort == %d))";

  char *current_filter = filter_string;
  size_t new_filter_size = strlen(current_filter) +
                           (proto == IPPROTO_UDP ? strlen(udp) : strlen(tcp)) +
                           16;
  char *new_filter = malloc(new_filter_size);

  strcpy(new_filter, current_filter);
  if (proto == IPPROTO_UDP)
    sprintf(new_filter + strlen(new_filter), udp, port, port);
  else
    sprintf(new_filter + strlen(new_filter), tcp, port, port);

  filter_string = new_filter;
  free(current_filter);
}

static void add_ip_id_str(int id) {
  char *newstr;
  const char *ipid = " or ip.Id == %d";
  char *addfilter = malloc(strlen(ipid) + 16);

  sprintf(addfilter, ipid, id);

  newstr = repl_str(filter_string, IPID_TEMPLATE, addfilter);
  free(filter_string);
  filter_string = newstr;

  newstr = repl_str(filter_passive_string, IPID_TEMPLATE, addfilter);
  free(filter_passive_string);
  filter_passive_string = newstr;
}

static void add_maxpayloadsize_str(unsigned short maxpayload) {
  char *newstr;
  /* 0x47455420 is "GET ", 0x504F5354 is "POST", big endian. */
  const char *maxpayloadsize_str =
      "and (tcp.PayloadLength ? tcp.PayloadLength < %hu "
      "or tcp.Payload32[0] == 0x47455420 or tcp.Payload32[0] == 0x504F5354 "
      "or (tcp.Payload[0] == 0x16 and tcp.Payload[1] == 0x03 and "
      "tcp.Payload[2] <= 0x03): true)";
  char *addfilter = malloc(strlen(maxpayloadsize_str) + 16);

  sprintf(addfilter, maxpayloadsize_str, maxpayload);

  newstr = repl_str(filter_string, MAXPAYLOADSIZE_TEMPLATE, addfilter);
  free(filter_string);
  filter_string = newstr;
}

static void finalize_filter_strings() {
  char *newstr, *newstr2;
  if (!filter_string) return;

  newstr2 = repl_str(filter_string, IPID_TEMPLATE, "");
  if (!newstr2) return;
  
  newstr = repl_str(newstr2, MAXPAYLOADSIZE_TEMPLATE, "");
  if (newstr) {
      free(filter_string);
      filter_string = newstr;
  }
  free(newstr2);

  if (filter_passive_string) {
    newstr = repl_str(filter_passive_string, IPID_TEMPLATE, "");
    if (newstr) {
        free(filter_passive_string);
        filter_passive_string = newstr;
    }
  }
}

static char *dumb_memmem(const char *haystack, unsigned int hlen,
                         const char *needle, unsigned int nlen) {
  // naive implementation
  if (nlen > hlen)
    return NULL;
  size_t i;
  for (i = 0; i < hlen - nlen + 1; i++) {
    if (memcmp(haystack + i, needle, nlen) == 0) {
      return (char *)(haystack + i);
    }
  }
  return NULL;
}

unsigned short int atousi(const char *str, const char *msg) {
  long unsigned int res = strtoul(str, NULL, 10u);
  enum { limitValue = 0xFFFFu };

  if (res > limitValue) {
    printf("%s\n", msg);
    return 0;
  }
  return (unsigned short int)res;
}

BYTE atoub(const char *str, const char *msg) {
  long unsigned int res = strtoul(str, NULL, 10u);
  enum { limitValue = 0xFFu };

  if (res > limitValue) {
    printf("%s\n", msg);
    return 0;
  }
  return (BYTE)res;
}

static HANDLE init(char *filter, UINT64 flags) {
  LPTSTR errormessage = NULL;
  DWORD errorcode = 0;
  filter = WinDivertOpen(filter, WINDIVERT_LAYER_NETWORK, 0, flags);
  if (filter != INVALID_HANDLE_VALUE)
    return filter;
  errorcode = GetLastError();
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                    FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, errorcode, MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
                (LPTSTR)&errormessage, 0, NULL);
  printf("Error opening filter: %d %s\n", errorcode, errormessage);
  LocalFree(errormessage);
  if (errorcode == 2)
    printf("The driver files WinDivert32.sys or WinDivert64.sys were not "
           "found.\n");
  else if (errorcode == 654)
    printf(
        "An incompatible version of the WinDivert driver is currently loaded.\n"
        "Please unload it with the following commands ran as administrator:\n\n"
        "sc stop windivert\n"
        "sc delete windivert\n"
        "sc stop windivert14\n"
        "sc delete windivert14\n");
  else if (errorcode == 1275)
    printf("This error occurs for various reasons, including:\n"
           "the WinDivert driver is blocked by security software; or\n"
           "you are using a virtualization environment that does not support "
           "drivers.\n");
  else if (errorcode == 1753)
    printf("This error occurs when the Base Filtering Engine service has been "
           "disabled.\n"
           "Enable Base Filtering Engine service.\n");
  else if (errorcode == 577)
    printf("Could not load driver due to invalid digital signature.\n"
           "Windows Server 2016 systems must have secure boot disabled to be \n"
           "able to load WinDivert driver.\n"
           "Windows 7 systems must be up-to-date or at least have KB3033929 "
           "installed.\n"
           "https://www.microsoft.com/en-us/download/details.aspx?id=46078\n\n"
           "WARNING! If you see this error on Windows 7, it means your system "
           "is horribly "
           "outdated and SHOULD NOT BE USED TO ACCESS THE INTERNET!\n"
           "Most probably, you don't have security patches installed and "
           "anyone in you LAN or "
           "public Wi-Fi network can get full access to your computer "
           "(MS17-010 and others).\n"
           "You should install updates IMMEDIATELY.\n");
  return NULL;
}

static int deinit(int index) {
  if (index >= 0 && index < MAX_FILTERS && filters[index]) {
    WinDivertShutdown(filters[index], WINDIVERT_SHUTDOWN_BOTH);
    WinDivertClose(filters[index]);
    filters[index] = NULL;
    return TRUE;
  }
  return FALSE;
}

void deinit_all() {
  for (int i = 0; i < MAX_FILTERS; i++) {
    deinit(i);
  }
}

void shutdown_hayalet() {
  for (int i = 0; i < filter_num; i++) {
    if (filters[i]) {
      WinDivertShutdown(filters[i], WINDIVERT_SHUTDOWN_BOTH);
    }
  }
}

void stop_hayalet() { exiting = 1; }

void reset_hayalet_state() {
  exiting = 0;
  deinit_all();
  if (filter_string) {
    free(filter_string);
    filter_string = NULL;
  }
  if (filter_passive_string) {
    free(filter_passive_string);
    filter_passive_string = NULL;
  }
  
  // Fully reset engine settings and flags
  memset(&engine, 0, sizeof(engine));

  // Re-initialize templates immediately
  filter_string = strdup(FILTER_STRING_TEMPLATE);
  filter_passive_string = strdup(FILTER_PASSIVE_STRING_TEMPLATE);

  optind = 1; // reset getopt state
  filter_num = 0;
  memset(filters, 0, sizeof(filters));

  packets_processed = 0;
  packets_circumvented = 0;
  // Clear sub-module state
  blackwhitelist_clear();
  dnsredir_clear();
  ttltrack_clear();
  fakepackets_clear();
}

static void sigint_handler(int sig __attribute__((unused))) { stop_hayalet(); }

static void mix_case(char *pktdata, unsigned int pktlen) {
  unsigned int i;

  if (pktlen <= 0)
    return;
  for (i = 0; i < pktlen; i++) {
    if (i % 2) {
      pktdata[i] = (char)toupper(pktdata[i]);
    }
  }
}

static int is_passivedpi_redirect(const char *pktdata, unsigned int pktlen) {
  /* First check if this is HTTP 302 redirect */
  if (memcmp(pktdata, http11_redirect_302, sizeof(http11_redirect_302) - 1) ==
          0 ||
      memcmp(pktdata, http10_redirect_302, sizeof(http10_redirect_302) - 1) ==
          0) {
    /* Then check if this is a redirect to new http site with Connection: close
     */
    if (dumb_memmem(pktdata, pktlen, location_http,
                    sizeof(location_http) - 1) &&
        dumb_memmem(pktdata, pktlen, connection_close,
                    sizeof(connection_close) - 1)) {
      return TRUE;
    }
  }
  return FALSE;
}

static int find_header_and_get_info(const char *pktdata, unsigned int pktlen,
                                    const char *hdrname, char **hdrnameaddr,
                                    char **hdrvalueaddr,
                                    unsigned int *hdrvaluelen) {
  char *data_addr_rn;
  char *hdr_begin;

  *hdrvaluelen = 0u;
  *hdrnameaddr = NULL;
  *hdrvalueaddr = NULL;

  /* Search for the header */
  hdr_begin = dumb_memmem(pktdata, pktlen, hdrname, strlen(hdrname));
  if (!hdr_begin)
    return FALSE;
  if (pktdata > hdr_begin)
    return FALSE;

  /* Set header address */
  *hdrnameaddr = hdr_begin;
  *hdrvalueaddr = hdr_begin + strlen(hdrname);

  /* Search for header end (\r\n) */
  data_addr_rn = dumb_memmem(
      *hdrvalueaddr, pktlen - (uintptr_t)(*hdrvalueaddr - pktdata), "\r\n", 2);
  if (data_addr_rn) {
    *hdrvaluelen = (uintptr_t)(data_addr_rn - *hdrvalueaddr);
    if (*hdrvaluelen >= 3 && *hdrvaluelen <= HOST_MAXLEN)
      return TRUE;
  }
  return FALSE;
}

/**
 * Very crude Server Name Indication (TLS ClientHello hostname) extractor.
 */
static int extract_sni(const char *pktdata, unsigned int pktlen,
                       char **hostnameaddr, unsigned int *hostnamelen) {
  unsigned int ptr = 0;
  unsigned const char *d = (unsigned const char *)pktdata;
  unsigned const char *hnaddr = 0;
  int hnlen = 0;

  while (ptr + 8 < pktlen) {
    /* Search for specific Extensions sequence */
    if (d[ptr] == '\0' && d[ptr + 1] == '\0' && d[ptr + 2] == '\0' &&
        d[ptr + 4] == '\0' && d[ptr + 6] == '\0' && d[ptr + 7] == '\0' &&
        /* Check Extension length, Server Name list length
         *  and Server Name length relations
         */
        d[ptr + 3] - d[ptr + 5] == 2 && d[ptr + 5] - d[ptr + 8] == 3) {
      if (ptr + 8 + d[ptr + 8] > pktlen) {
        return FALSE;
      }
      hnaddr = &d[ptr + 9];
      hnlen = d[ptr + 8];
      /* Limit hostname size up to 253 bytes */
      if (hnlen < 3 || hnlen > HOST_MAXLEN) {
        return FALSE;
      }
      /* Validate that hostname has only ascii lowercase characters */
      for (int i = 0; i < hnlen; i++) {
        if (!((hnaddr[i] >= '0' && hnaddr[i] <= '9') ||
              (hnaddr[i] >= 'a' && hnaddr[i] <= 'z') || hnaddr[i] == '.' ||
              hnaddr[i] == '-')) {
          return FALSE;
        }
      }
      *hostnameaddr = (char *)hnaddr;
      *hostnamelen = (unsigned int)hnlen;
      return TRUE;
    }
    ptr++;
  }
  return FALSE;
}

static inline void change_window_size(const PWINDIVERT_TCPHDR ppTcpHdr,
                                      unsigned int size) {
  if (size >= 1 && size <= 0xFFFFu) {
    ppTcpHdr->Window = htons((u_short)size);
  }
}

/* HTTP method end without trailing space */
static PVOID find_http_method_end(const char *pkt, unsigned int http_frag,
                                  int *is_fragmented) {
  unsigned int i;
  for (i = 0; i < (sizeof(http_methods) / sizeof(*http_methods)); i++) {
    if (memcmp(pkt, http_methods[i], strlen(http_methods[i])) == 0) {
      if (is_fragmented)
        *is_fragmented = 0;
      return (char *)pkt + strlen(http_methods[i]) - 1;
    }
    /* Try to find HTTP method in a second part of fragmented packet */
    if ((http_frag == 1 || http_frag == 2) &&
        memcmp(pkt, http_methods[i] + http_frag,
               strlen(http_methods[i]) - http_frag) == 0) {
      if (is_fragmented)
        *is_fragmented = 1;
      return (char *)pkt + strlen(http_methods[i]) - http_frag - 1;
    }
  }
  return NULL;
}

/** Fragment and send the packet.
 *
 * This function cuts off the end of the packet (step=0) or
 * the beginning of the packet (step=1) with fragment_size bytes.
 */
static void
send_native_fragment(HANDLE hFilter, WINDIVERT_ADDRESS addr, char *packet,
                     UINT packetLen, PVOID packet_data, UINT packet_dataLen,
                     int packet_v4, int packet_v6, PWINDIVERT_IPHDR ppIpHdr,
                     PWINDIVERT_IPV6HDR ppIpV6Hdr, PWINDIVERT_TCPHDR ppTcpHdr,
                     unsigned int fragment_size, int step) {
  char packet_bak[MAX_PACKET_SIZE];
  memcpy(packet_bak, packet, packetLen);
  UINT orig_packetLen = packetLen;

  if (fragment_size >= packet_dataLen) {
    if (step == 1)
      fragment_size = 0;
    else
      return;
  }

  if (step == 0) {
    if (packet_v4)
      ppIpHdr->Length =
          htons(ntohs(ppIpHdr->Length) - packet_dataLen + fragment_size);
    else if (packet_v6)
      ppIpV6Hdr->Length =
          htons(ntohs(ppIpV6Hdr->Length) - packet_dataLen + fragment_size);
    // printf("step0 (%d:%d), pp:%d, was:%d, now:%d\n",
    //                 packet_v4, packet_v6, ntohs(ppIpHdr->Length),
    //                 packetLen, packetLen - packet_dataLen + fragment_size);
    packetLen = packetLen - packet_dataLen + fragment_size;
  }

  else if (step == 1) {
    if (packet_v4)
      ppIpHdr->Length = htons(ntohs(ppIpHdr->Length) - fragment_size);
    else if (packet_v6)
      ppIpV6Hdr->Length = htons(ntohs(ppIpV6Hdr->Length) - fragment_size);
    // printf("step1 (%d:%d), pp:%d, was:%d, now:%d\n", packet_v4, packet_v6,
    // ntohs(ppIpHdr->Length),
    //                 packetLen, packetLen - fragment_size);
    memmove(packet_data, (char *)packet_data + fragment_size,
            packet_dataLen - fragment_size);
    packetLen -= fragment_size;

    ppTcpHdr->SeqNum = htonl(ntohl(ppTcpHdr->SeqNum) + fragment_size);
  }

  addr.IPChecksum = 0;
  addr.TCPChecksum = 0;

  WinDivertHelperCalcChecksums(packet, packetLen, &addr, 0);
  WinDivertSend(hFilter, packet, packetLen, NULL, &addr);
  memcpy(packet, packet_bak, orig_packetLen);
  // printf("Sent native fragment of %d size (step%d)\n", packetLen, step);
}
typedef enum {
    unknown,
    ipv4_tcp, ipv4_tcp_data, ipv4_udp_data,
    ipv6_tcp, ipv6_tcp_data, ipv6_udp_data
} packet_type_t;

void ApplyEngineMode(int mode) {
    // Mode application logic (No state reset here!)
    if (mode == 0) return;
    if (mode == -1) {
        do_passivedpi = do_host = do_host_removespace = do_fragment_http =
            do_fragment_https = do_fragment_http_persistent =
                do_fragment_http_persistent_nowait = 1;
    } else if (mode == -2) {
        do_passivedpi = do_host = do_host_removespace = do_fragment_http =
            do_fragment_https = do_fragment_http_persistent =
                do_fragment_http_persistent_nowait = 1;
        https_fragment_size = 40u;
    } else if (mode == -3) {
        do_passivedpi = do_host = do_host_removespace = do_fragment_https = 1;
        https_fragment_size = 40u;
    } else if (mode == -4) {
        do_passivedpi = do_host = do_host_removespace = 1;
    } else if (mode == -5) {
        do_fragment_http = do_fragment_https = 1;
        do_reverse_frag = do_native_frag = 1;
        http_fragment_size = https_fragment_size = 2;
        do_fragment_http_persistent = do_fragment_http_persistent_nowait = 1;
        do_fake_packet = 1; do_auto_ttl = 1; max_payload_size = 1200;
    } else if (mode == -6) {
        do_fragment_http = do_fragment_https = 1;
        do_reverse_frag = do_native_frag = 1;
        http_fragment_size = https_fragment_size = 2;
        do_fragment_http_persistent = do_fragment_http_persistent_nowait = 1;
        do_fake_packet = 1; do_wrong_seq = 1; max_payload_size = 1200;
    } else if (mode <= -7) {
        do_fragment_http = do_fragment_https = 1;
        do_reverse_frag = do_native_frag = 1;
        http_fragment_size = https_fragment_size = 2;
        do_fragment_http_persistent = do_fragment_http_persistent_nowait = 1;
        do_fake_packet = 1; do_wrong_chksum = 1; max_payload_size = 1200;
        if (mode <= -8) do_wrong_seq = 1;
        if (mode == -9) do_block_quic = 1;
    }
}

int Hayalet_RunConfig(HayaletConfig config) {
    reset_hayalet_state();
    
    active_filter_mode = config.cfg_filter_mode;
    allowlist_enabled = config.cfg_allowlist_enabled;
    ApplyEngineMode(config.cfg_mode);

    if (strlen(config.cfg_dns_addr) > 0) {
        do_dnsv4_redirect = 1; inet_pton(AF_INET, config.cfg_dns_addr, &dnsv4_addr);
        dnsv4_port = htons(config.cfg_dns_port ? (uint16_t)config.cfg_dns_port : 53);
        add_filter_str(IPPROTO_UDP, ntohs(dnsv4_port));
    }
    if (strlen(config.cfg_dnsv6_addr) > 0) {
        do_dnsv6_redirect = 1; inet_pton(AF_INET6, config.cfg_dnsv6_addr, dnsv6_addr.s6_addr);
        dnsv6_port = htons(config.cfg_dnsv6_port ? (uint16_t)config.cfg_dnsv6_port : 53);
        add_filter_str(IPPROTO_UDP, ntohs(dnsv6_port));
    }
    if (config.cfg_filter_mode == 1 && strlen(config.cfg_blacklist_path) > 0) {
        do_blacklist = 1; blackwhitelist_load_list(config.cfg_blacklist_path);
    }
    return run_hayalet(0, NULL);
}

int run_hayalet(int argc, char *argv[]) {
  packet_type_t packet_type;
  int i, should_reinject, should_recalc_checksum = 0;
  int sni_ok = 0;
  int opt;
  int packet_v4, packet_v6;
  WINDIVERT_ADDRESS addr;
  char packet[MAX_PACKET_SIZE];
  PVOID packet_data;
  UINT packetLen;
  UINT packet_dataLen;
  PWINDIVERT_IPHDR ppIpHdr;
  PWINDIVERT_IPV6HDR ppIpV6Hdr;
  PWINDIVERT_TCPHDR ppTcpHdr;
  PWINDIVERT_UDPHDR ppUdpHdr;
  conntrack_info_t dns_conn_info;
  tcp_conntrack_info_t tcp_conn_info;

  // Initialize templates if not already set by Hayalet_RunConfig
  if (!filter_string) filter_string = strdup(FILTER_STRING_TEMPLATE);
  if (!filter_passive_string) filter_passive_string = strdup(FILTER_PASSIVE_STRING_TEMPLATE);

  // Bridge loop for CLI compatibility (Legacy)
  if (argc > 0) {
      if (argc == 1) ApplyEngineMode(-9); // Default mode if no args
      optind = 1;
      while ((opt = getopt_long(argc, argv, "123456789pqrsaf:e:mwk:nA:b:d:g:i:!:@:v:u:}:j:t:x:M:", long_options, NULL)) != -1) {
          switch (opt) {
              case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                  ApplyEngineMode(-(opt - '0')); break;
              case 'p': do_passivedpi = 1; break;
              case 'q': do_block_quic = 1; break;
              case 'r': do_host = 1; break;
              case 's': do_host_removespace = 1; break;
              case 'a': do_additional_space = 1; do_host_removespace = 1; break;
              case 'm': do_host_mixedcase = 1; break;
              case 'f': do_fragment_http = 1; SET_HTTP_FRAGMENT_SIZE_OPTION(atousi(optarg, "ERR")); break;
              case 'k': do_fragment_http_persistent = 1; do_native_frag = 1; SET_HTTP_FRAGMENT_SIZE_OPTION(atousi(optarg, "ERR")); break;
              case 'n': do_fragment_http_persistent_nowait = 1; do_native_frag = 1; break;
              case 'e': do_fragment_https = 1; https_fragment_size = atousi(optarg, "ERR"); break;
              case 'w': do_http_allports = 1; break;
              case 'z': { int p = atoi(optarg); if (p > 0 && p != 80 && p != 443) add_filter_str(IPPROTO_TCP, p); break; }
              case 'i': add_ip_id_str(atousi(optarg, "ERR")); break;
              case 'd': do_dnsv4_redirect = 1; inet_pton(AF_INET, optarg, &dnsv4_addr); add_filter_str(IPPROTO_UDP, 53); flush_dns_cache(); break;
              case 'g': if (do_dnsv4_redirect) { dnsv4_port = htons((uint16_t)atousi(optarg, "ERR")); if (ntohs(dnsv4_port) != 53) add_filter_str(IPPROTO_UDP, ntohs(dnsv4_port)); } break;
              case '!': do_dnsv6_redirect = 1; inet_pton(AF_INET6, optarg, dnsv6_addr.s6_addr); add_filter_str(IPPROTO_UDP, 53); flush_dns_cache(); break;
              case '@': if (do_dnsv6_redirect) { dnsv6_port = htons((uint16_t)atousi(optarg, "ERR")); if (ntohs(dnsv6_port) != 53) add_filter_str(IPPROTO_UDP, ntohs(dnsv6_port)); } break;
              case 'v': do_dns_verb = 1; do_tcp_verb = 1; break;
              case 'b': do_blacklist = 1; blackwhitelist_clear(); blackwhitelist_load_list(optarg); break;
              case ']': do_allow_no_sni = 1; break;
              case '>': do_fragment_by_sni = 1; break;
              case '$': do_auto_ttl = 0; do_fake_packet = 1; ttl_of_fake_packet = atoub(optarg, "ERR"); break;
              case '[': do_fake_packet = 1; ttl_min_nhops = atoub(optarg, "ERR"); break;
              case '+': do_fake_packet = 1; do_auto_ttl = 1; break;
              case '%': do_fake_packet = 1; do_wrong_chksum = 1; break;
              case ')': do_fake_packet = 1; do_wrong_seq = 1; break;
              case '*': do_native_frag = 1; break;
              case '(': do_reverse_frag = 1; do_native_frag = 1; break;
              case '|': max_payload_size = (uint16_t)atousi(optarg ? optarg : "1200", "ERR"); break;
              case 'u': fake_load_from_hex(optarg); break;
              case '}': fake_load_from_sni(optarg); break;
              case 'j': fake_load_random(atoub(optarg, "ERR"), 200); break;
              case 't': fakes_resend = atoub(optarg, "ERR"); break;
              case 'x': debug_exit = true; break;
              case 'M': active_filter_mode = atoi(optarg); break;
              case 'A': allowlist_enabled = atoi(optarg); break;
          }
      }
  }

  // Common Initialization Logic

  char *hdr_name_addr = NULL, *hdr_value_addr = NULL;
  unsigned int hdr_value_len;

  // Library & Driver Loading Path Setup
  char exeDir[MAX_PATH];
  GetModuleFileName(NULL, exeDir, MAX_PATH);
  char *lastSlash = strrchr(exeDir, '\\');
  if (lastSlash) { *lastSlash = '\0'; strcat(exeDir, "\\libs"); SetDllDirectory(exeDir); }
  else { SetDllDirectory("libs"); }
  
  SetSearchPathMode(BASE_SEARCH_PATH_ENABLE_SAFE_SEARCHMODE | BASE_SEARCH_PATH_PERMANENT);
  _mkdir("userfiles");

  if (!running_from_service) {
    running_from_service = 1;
    if (service_register(argc, argv)) return 0; // Legacy service behavior
    running_from_service = 0;
  }

  if (do_fake_packet && !fakes_resend) fakes_resend = 1;

  if (!http_fragment_size)
    http_fragment_size = 2;
  if (!https_fragment_size)
    https_fragment_size = 2;
  if (!auto_ttl_1)
    auto_ttl_1 = 1;
  if (!auto_ttl_2)
    auto_ttl_2 = 4;
  if (do_auto_ttl) {
    if (!ttl_min_nhops)
      ttl_min_nhops = 3;
    if (!auto_ttl_max)
      auto_ttl_max = 10;
  }

  printf("\n"
         " Hayalet Engine %s | Stealth DPI Circumvention\n"
         " --------------------------------------------------------\n"
         " [Protection] Passive DPI Blocking  : [%s]\n"
         " [Protocol]   QUIC/HTTP3 Blocking   : [%s]\n"
         " [Fragment]   HTTP Segment Size     : %u\n"
         " [Fragment]   Keep-Alive Segment    : %u\n"
         " [Fragment]   HTTPS Segment Size    : %u\n"
         " [Fragment]   SNI-Aware Splitting   : [%s]\n"
         " [Network]    Native Fragmentation  : [%s]\n"
         " [Network]    Reverse Frag Order    : [%s]\n"
         " [HTTP]       hoSt Header Trick     : [%s]\n"
         " [HTTP]       No Space Trick        : [%s]\n"
         " [HTTP]       Additional Space      : [%s]\n"
         " [HTTP]       Case Randomization    : [%s]\n"
         " [HTTP]       All Port Inspection   : [%s]\n"
         " [DNS]        IPv4 Redirection      : [%s]\n"
         " [DNS]        IPv6 Redirection      : [%s]\n"
         " [Fake]       SNI Masquerade Mode   : [%s]\n"
         " [Fake]       Auto TTL Range        : %hu-%hu-%hu\n"
         " [Fake]       Resend Cycles         : %d\n"
         " [Engine]     Max Scan Payload      : %hu bytes\n\n",
         HAYALET_VERSION, do_passivedpi ? "ON" : "OFF",
         do_block_quic ? "ON" : "OFF",
         (do_fragment_http ? http_fragment_size : 0),
         (do_fragment_http_persistent ? http_fragment_size : 0),
         (do_fragment_https ? https_fragment_size : 0),
         do_fragment_by_sni ? "YES" : "NO", do_native_frag ? "ACTIVE" : "OFF",
         do_reverse_frag ? "YES" : "NO", do_host ? "ON" : "OFF",
         do_host_removespace ? "ON" : "OFF", do_additional_space ? "ON" : "OFF",
         do_host_mixedcase ? "ON" : "OFF", do_http_allports ? "ON" : "OFF",
         do_dnsv4_redirect ? "YES" : "NO", do_dnsv6_redirect ? "YES" : "NO",
         do_auto_ttl ? "AUTO" : (do_fake_packet ? "FIXED" : "OFF"), auto_ttl_1,
         auto_ttl_2, auto_ttl_max, fakes_resend, max_payload_size);

  if (do_fragment_http && http_fragment_size > 2 && !do_native_frag) {
    puts("\nWARNING: HTTP fragmentation values > 2 are not fully compatible "
         "with other options. Please use values <= 2 or disable HTTP "
         "fragmentation "
         "completely.");
  }

  if (do_native_frag && !(do_fragment_http || do_fragment_https)) {
    puts("\nERROR: Native fragmentation is enabled but fragment sizes are not "
         "set.\n"
         "Fragmentation has no effect.");
    return 1;
  }

  if (max_payload_size)
    add_maxpayloadsize_str(max_payload_size);
  finalize_filter_strings();
  puts("Initializing WinDivert Filter sync...");
  filter_num = 0;

  if (do_passivedpi) {
    /* IPv4 only filter for inbound RST packets with ID [0x0; 0xF] */
    filters[filter_num] = init(filter_passive_string, WINDIVERT_FLAG_DROP);
    if (filters[filter_num] == NULL)
      return 1;
    filter_num++;
  }

  if (do_block_quic) {
    filters[filter_num] = init(FILTER_PASSIVE_BLOCK_QUIC, WINDIVERT_FLAG_DROP);
    if (filters[filter_num] == NULL)
      return 1;
    filter_num++;
  }

  // Primary active filter for DPI circumvention
  filters[filter_num] = init(filter_string, 0);
  if (filters[filter_num] == NULL)
      return 1;
  w_filter = filters[filter_num]; // This handle is used for WinDivertRecv/Send
  filter_num++;

  for (i = 0; i < filter_num; i++) {
    if (filters[i] == NULL)
      return 1;
  }
  if (debug_exit) {
    printf("Debug Exit\n");
    return EXIT_SUCCESS;
  }
  printf("[OK] Filter sync successful. Hayalet Engine is active!\n");
  signal(SIGINT, sigint_handler);

  LoadFilterApps();
  if (active_filter_mode == 2) {
    CreateThread(NULL, 0, flow_monitor_thread, NULL, 0, NULL);
  }

  LogWithTime("Hayalet Engine started (Filter Mode: %d)\n", active_filter_mode);
  while (!exiting) {
    if (WinDivertRecv(w_filter, packet, sizeof(packet), &packetLen, &addr)) {
      packets_processed++;
      uint32_t pid = 0;

      debug("Got %s packet, len=%d!\n", addr.Outbound ? "outbound" : "inbound",
            packetLen);
      should_reinject = 1;
      should_recalc_checksum = 0;
      sni_ok = 0;

      ppIpHdr = (PWINDIVERT_IPHDR)NULL;
      ppIpV6Hdr = (PWINDIVERT_IPV6HDR)NULL;
      ppTcpHdr = (PWINDIVERT_TCPHDR)NULL;
      ppUdpHdr = (PWINDIVERT_UDPHDR)NULL;
      packet_v4 = packet_v6 = 0;
      packet_type = unknown;

      // Parse network packet and set it's type
      if (WinDivertHelperParsePacket(packet, packetLen, &ppIpHdr, &ppIpV6Hdr,
                                     NULL, NULL, NULL, &ppTcpHdr, &ppUdpHdr,
                                     &packet_data, &packet_dataLen, NULL,
                                     NULL)) {
        if (active_filter_mode == 2 || active_filter_mode == 3) {
          uint16_t srcport =
              (ppTcpHdr
                   ? WinDivertHelperNtohs(ppTcpHdr->SrcPort)
                   : (ppUdpHdr ? WinDivertHelperNtohs(ppUdpHdr->SrcPort) : 0));
          if (addr.Outbound) {
            if (ppTcpHdr)
              pid = tcp_port_map[srcport].pid;
            else if (ppUdpHdr)
              pid = udp_port_map[srcport].pid;

            // Fallback if PID is missing
            if (pid == 0 && srcport != 0) {
              pid =
                  GetPidFromPort(srcport, ppTcpHdr ? IPPROTO_TCP : IPPROTO_UDP);
              // Update map to avoid future lookups for this port
              if (ppTcpHdr)
                tcp_port_map[srcport].pid = pid;
              else
                udp_port_map[srcport].pid = pid;
            }
          }

          LoadFilterApps(); // Checks internal timer before reloading

          // In Gaming Mode (3), we proceed for all packets to check SNI (for
          // browsers/others) Mode 2 still requires a valid and allowed PID to
          // proceed.
          if (active_filter_mode == 2 && (pid == 0 || !IsProcessAllowed(pid))) {
            WinDivertSend(w_filter, packet, packetLen, NULL, &addr);
            continue;
          }
        }

        if (ppIpHdr) {
          packet_v4 = 1;
          if (ppTcpHdr) {
            packet_type = ipv4_tcp;
            if (packet_data) {
              packet_type = ipv4_tcp_data;
            }
          } else if (ppUdpHdr && packet_data) {
            packet_type = ipv4_udp_data;
          }
        }

        else if (ppIpV6Hdr) {
          packet_v6 = 1;
          if (ppTcpHdr) {
            packet_type = ipv6_tcp;
            if (packet_data) {
              packet_type = ipv6_tcp_data;
            }
          } else if (ppUdpHdr && packet_data) {
            packet_type = ipv6_udp_data;
          }
        }
      }

      debug("packet_type: %d, packet_v4: %d, packet_v6: %d\n", packet_type,
            packet_v4, packet_v6);

      if (packet_type == ipv4_tcp_data || packet_type == ipv6_tcp_data) {
        // printf("Got parsed packet, len=%d!\n", packet_dataLen);
        /* Got a TCP packet WITH DATA */

        /* Handle INBOUND packet with data and find HTTP REDIRECT in there */
        if (!addr.Outbound && packet_dataLen > 16) {
          /* If INBOUND packet with DATA (tcp.Ack) */

          /* Drop packets from filter with HTTP 30x Redirect */
          if (do_passivedpi &&
              is_passivedpi_redirect(packet_data, packet_dataLen)) {
            if (packet_v4) {
              // printf("Dropping HTTP Redirect packet!\n");
              should_reinject = 0;
            } else if (packet_v6 &&
                       WINDIVERT_IPV6HDR_GET_FLOWLABEL(ppIpV6Hdr) == 0x0) {
              /* Contrary to IPv4 where we get only packets with IP ID 0x0-0xF,
               * for IPv6 we got all the incoming data packets since we can't
               * filter them in a driver.
               *
               * Handle only IPv6 Flow Label == 0x0 for now
               */
              // printf("Dropping HTTP Redirect packet!\n");
              should_reinject = 0;
            }
          }
        }
        /* Handle OUTBOUND packet on port 443, search for something that
         * resembles TLS handshake, send fake request.
         */
        else if (addr.Outbound &&
                 ((do_fragment_https ? packet_dataLen == https_fragment_size
                                     : 0) ||
                  packet_dataLen > 16) &&
                 ppTcpHdr->DstPort != htons(80) &&
                 (do_fake_packet || do_native_frag)) {
          /**
           * In case of Window Size fragmentation=2, we'll receive only 2 byte
           * packet. But if the packet is more than 2 bytes, check ClientHello
           * byte.
           */
          if ((packet_dataLen == 2 &&
               memcmp(packet_data, "\x16\x03", 2) == 0) ||
              (packet_dataLen >= 3 &&
               (memcmp(packet_data, "\x16\x03\x01", 3) == 0 ||
                memcmp(packet_data, "\x16\x03\x03", 3) == 0))) {
            if (do_blacklist || do_fragment_by_sni) {
              sni_ok = extract_sni(packet_data, packet_dataLen, &host_addr,
                                   &host_len);
            }
            // Allowlist check: if domain is allowlisted, skip all DPI tricks
            if (allowlist_enabled && (active_filter_mode == 0 || active_filter_mode == 2)) {
              char *sni_tmp = NULL; unsigned int sni_len_tmp = 0;
              if (extract_sni(packet_data, packet_dataLen, &sni_tmp, &sni_len_tmp)) {
                if (IsAllowlisted(sni_tmp, sni_len_tmp)) {
                  WinDivertSend(w_filter, packet, packetLen, NULL, &addr);
                  continue;
                }
              }
            }
            if ((do_blacklist && sni_ok &&
                 blackwhitelist_check_hostname(host_addr, host_len)) ||
                (do_blacklist && !sni_ok && do_allow_no_sni) ||
                (!do_blacklist)) {

              char lsni[HOST_MAXLEN + 1] = {0};
              if (extract_sni(packet_data, packet_dataLen, &host_addr,
                              &host_len)) {
                if (host_len > HOST_MAXLEN)
                  host_len = HOST_MAXLEN;
                memcpy(lsni, host_addr, host_len);

                // Mode 3 (Gaming) - only allow certain hosts
                if (active_filter_mode == 3) {
                  if (!strstr(lsni, "roblox.com") &&
                      !strstr(lsni, "rbxcdn.com") && !strstr(lsni, "rbx.com") &&
                      !strstr(lsni, "discord.com") &&
                      !strstr(lsni, "discord.gg") &&
                      !strstr(lsni, "discordapp.net") &&
                      !strstr(lsni, "discordapp.com") &&
                      !strstr(lsni, "discord.media") &&
                      !strstr(lsni, "discordstatus.com") &&
                      !strstr(lsni, "discord.gift") &&
                      !strstr(lsni, "discord.co") && !IsProcessAllowed(pid)) {
                    WinDivertSend(w_filter, packet, packetLen, NULL, &addr);
                    continue;
                  }
                }

                char procName[MAX_PATH] = "Unknown";
                GetProcessNameByPid(pid, procName, sizeof(procName));
                LogWithTime("[SUCCESS] Bypass Applied | SNI: %s | App: %s\n",
                            lsni, procName);
              }

              if (do_fake_packet) {
                TCP_HANDLE_OUTGOING_FAKE_PACKET(send_fake_https_request);
              }
              if (do_native_frag) {
                // Signal for native fragmentation code handler
                should_recalc_checksum = 1;
              }
            }
          }
        }
        /* Handle OUTBOUND packet on port 80, search for Host header */
        else if (addr.Outbound && packet_dataLen > 16 &&
                 (do_http_allports ? 1 : (ppTcpHdr->DstPort == htons(80))) &&
                 find_http_method_end(
                     packet_data, (do_fragment_http ? http_fragment_size : 0u),
                     &http_req_fragmented) &&
                 (do_host || do_host_removespace || do_host_mixedcase ||
                  do_fragment_http_persistent || do_fake_packet)) {

          /* Find Host header */
          if (find_header_and_get_info(packet_data, packet_dataLen,
                                       http_host_find, &hdr_name_addr,
                                       &hdr_value_addr, &hdr_value_len) &&
              hdr_value_len > 0 && hdr_value_len <= HOST_MAXLEN &&
              (do_blacklist ? blackwhitelist_check_hostname(hdr_value_addr,
                                                            hdr_value_len)
                            : 1)) {
            host_addr = hdr_value_addr;
            host_len = hdr_value_len;
            char lhost[HOST_MAXLEN + 1] = {0};
            if (host_len > HOST_MAXLEN)
              host_len = HOST_MAXLEN;
            memcpy(lhost, host_addr, host_len);

            // Mode 3 (Gaming) - only allow certain hosts
            if (active_filter_mode == 3) {
              if (!strstr(lhost, "roblox.com") &&
                  !strstr(lhost, "rbxcdn.com") && !strstr(lhost, "rbx.com") &&
                  !strstr(lhost, "discord.com") &&
                  !strstr(lhost, "discord.gg") &&
                  !strstr(lhost, "discordapp.net") &&
                  !strstr(lhost, "discordapp.com") &&
                  !strstr(lhost, "discord.media") &&
                  !strstr(lhost, "discordstatus.com") &&
                  !strstr(lhost, "discord.gift") &&
                  !strstr(lhost, "discord.co") && !IsProcessAllowed(pid)) {
                WinDivertSend(w_filter, packet, packetLen, NULL, &addr);
                continue;
              }
            }

            char procName[MAX_PATH] = "Unknown";
            GetProcessNameByPid(pid, procName, sizeof(procName));
            LogWithTime("[SUCCESS] Bypass Applied | Host: %s | App: %s\n",
                        lhost, procName);

            if (do_native_frag) {
              // Signal for native fragmentation code handler
              should_recalc_checksum = 1;
            }

            if (do_fake_packet) {
              TCP_HANDLE_OUTGOING_FAKE_PACKET(send_fake_http_request);
            }

            if (do_host_mixedcase) {
              mix_case(host_addr, host_len);
              should_recalc_checksum = 1;
            }

            if (do_host) {
              /* Replace "Host: " with "hoSt: " */
              memcpy(hdr_name_addr, http_host_replace,
                     strlen(http_host_replace));
              should_recalc_checksum = 1;
              // printf("Replaced Host header!\n");
            }

            /* If removing space between host header and its value
             * and adding additional space between Method and Request-URI */
            if (do_additional_space && do_host_removespace) {
              /* End of "Host:" without trailing space */
              method_addr = find_http_method_end(
                  packet_data, (do_fragment_http ? http_fragment_size : 0),
                  NULL);

              if (method_addr) {
                memmove(method_addr + 1, method_addr,
                        (size_t)(host_addr - method_addr - 1));
                should_recalc_checksum = 1;
              }
            }
            /* If just removing space between host header and its value */
            else if (do_host_removespace) {
              if (find_header_and_get_info(packet_data, packet_dataLen,
                                           http_useragent_find, &hdr_name_addr,
                                           &hdr_value_addr, &hdr_value_len)) {
                useragent_addr = hdr_value_addr;
                useragent_len = hdr_value_len;

                /* We move Host header value by one byte to the left and then
                 * "insert" stolen space to the end of User-Agent value because
                 * some web servers are not tolerant to additional space in the
                 * end of Host header.
                 *
                 * Nothing is done if User-Agent header is missing.
                 */
                if (useragent_addr && useragent_len > 0) {
                  /* useragent_addr is in the beginning of User-Agent value */

                  if (useragent_addr > host_addr) {
                    /* Move one byte to the LEFT from "Host:"
                     * to the end of User-Agent
                     */
                    memmove(
                        host_addr - 1, host_addr,
                        (size_t)(useragent_addr + useragent_len - host_addr));
                    host_addr -= 1;
                    /* Put space in the end of User-Agent header */
                    *(char *)((unsigned char *)useragent_addr + useragent_len -
                              1) = ' ';
                    should_recalc_checksum = 1;
                    // printf("Replaced Host header!\n");
                  } else {
                    /* User-Agent goes BEFORE Host header */

                    /* Move one byte to the RIGHT from the end of User-Agent
                     * to the "Host:"
                     */
                    memmove(useragent_addr + useragent_len + 1,
                            useragent_addr + useragent_len,
                            (size_t)(host_addr - 1 -
                                     (useragent_addr + useragent_len)));
                    /* Put space in the end of User-Agent header */
                    *(char *)((unsigned char *)useragent_addr + useragent_len) =
                        ' ';
                    should_recalc_checksum = 1;
                    // printf("Replaced Host header!\n");
                  }
                } /* if (host_len <= HOST_MAXLEN && useragent_addr) */
              } /* if (find_header_and_get_info http_useragent) */
            } /* else if (do_host_removespace) */
          } /* if (find_header_and_get_info http_host) */
        } /* Handle OUTBOUND packet with data */

        /*
         * should_recalc_checksum mean we have detected a packet to handle and
         * modified it in some way.
         * Handle native fragmentation here, incl. sending the packet.
         */
        if (should_reinject && should_recalc_checksum && do_native_frag) {
          current_fragment_size = 0;
          if (do_fragment_http && ppTcpHdr->DstPort == htons(80)) {
            current_fragment_size = http_fragment_size;
          } else if (do_fragment_https && ppTcpHdr->DstPort != htons(80)) {
            if (do_fragment_by_sni && sni_ok) {
              current_fragment_size = (void *)host_addr - packet_data;
            } else {
              current_fragment_size = https_fragment_size;
            }
          }

          if (current_fragment_size) {
            send_native_fragment(w_filter, addr, packet, packetLen, packet_data,
                                 packet_dataLen, packet_v4, packet_v6, ppIpHdr,
                                 ppIpV6Hdr, ppTcpHdr, current_fragment_size,
                                 do_reverse_frag);

            send_native_fragment(w_filter, addr, packet, packetLen, packet_data,
                                 packet_dataLen, packet_v4, packet_v6, ppIpHdr,
                                 ppIpV6Hdr, ppTcpHdr, current_fragment_size,
                                 !do_reverse_frag);
            continue;
          }
        }
      } /* Handle TCP packet with data */

      /* Else if we got TCP packet without data */
      else if (packet_type == ipv4_tcp || packet_type == ipv6_tcp) {
        /* If we got INBOUND SYN+ACK packet */
        if (!addr.Outbound && ppTcpHdr->Syn == 1 && ppTcpHdr->Ack == 1) {
          // printf("Changing Window Size!\n");
          /*
           * Window Size is changed even if do_fragment_http_persistent
           * is enabled as there could be non-HTTP data on port 80
           */

          if (do_fake_packet && (do_auto_ttl || ttl_min_nhops)) {
            if (!((packet_v4 &&
                   tcp_handle_incoming(&ppIpHdr->SrcAddr, &ppIpHdr->DstAddr,
                                       ppTcpHdr->SrcPort, ppTcpHdr->DstPort, 0,
                                       ppIpHdr->TTL)) ||
                  (packet_v6 &&
                   tcp_handle_incoming((uint32_t *)&ppIpV6Hdr->SrcAddr,
                                       (uint32_t *)&ppIpV6Hdr->DstAddr,
                                       ppTcpHdr->SrcPort, ppTcpHdr->DstPort, 1,
                                       ppIpV6Hdr->HopLimit)))) {
              if (do_tcp_verb)
                puts("[TCP WARN] Can't add TCP connection record.");
            }
          }

          if (!do_native_frag) {
            if (do_fragment_http && ppTcpHdr->SrcPort == htons(80)) {
              change_window_size(ppTcpHdr, http_fragment_size);
              should_recalc_checksum = 1;
            } else if (do_fragment_https && ppTcpHdr->SrcPort != htons(80)) {
              change_window_size(ppTcpHdr, https_fragment_size);
              should_recalc_checksum = 1;
            }
          }
        }
      }

      /* Else if we got UDP packet with data */
      else if ((do_dnsv4_redirect && (packet_type == ipv4_udp_data)) ||
               (do_dnsv6_redirect && (packet_type == ipv6_udp_data))) {
        if (!addr.Outbound) {
          if ((packet_v4 &&
               dns_handle_incoming(&ppIpHdr->DstAddr, ppUdpHdr->DstPort,
                                   packet_data, packet_dataLen, &dns_conn_info,
                                   0)) ||
              (packet_v6 &&
               dns_handle_incoming(ppIpV6Hdr->DstAddr, ppUdpHdr->DstPort,
                                   packet_data, packet_dataLen, &dns_conn_info,
                                   1))) {
            /* Changing source IP and port to the values
             * from DNS conntrack */
            if (packet_v4)
              ppIpHdr->SrcAddr = dns_conn_info.dstip[0];
            else if (packet_v6)
              ipv6_copy_addr(ppIpV6Hdr->SrcAddr, dns_conn_info.dstip);
            ppUdpHdr->DstPort = dns_conn_info.srcport;
            ppUdpHdr->SrcPort = dns_conn_info.dstport;
            should_recalc_checksum = 1;
          } else {
            if (dns_is_dns_packet(packet_data, packet_dataLen, 0))
              should_reinject = 0;

            if (do_dns_verb && !should_reinject) {
              printf("[DNS] Error handling incoming packet: srcport = %hu, "
                     "dstport = %hu\n",
                     ntohs(ppUdpHdr->SrcPort), ntohs(ppUdpHdr->DstPort));
            }
          }
        }

        else if (addr.Outbound) {
          if ((packet_v4 &&
               dns_handle_outgoing(&ppIpHdr->SrcAddr, ppUdpHdr->SrcPort,
                                   &ppIpHdr->DstAddr, ppUdpHdr->DstPort,
                                   packet_data, packet_dataLen, 0)) ||
              (packet_v6 &&
               dns_handle_outgoing(ppIpV6Hdr->SrcAddr, ppUdpHdr->SrcPort,
                                   ppIpV6Hdr->DstAddr, ppUdpHdr->DstPort,
                                   packet_data, packet_dataLen, 1))) {
            /* Changing destination IP and port to the values
             * from configuration */
            if (packet_v4) {
              ppIpHdr->DstAddr = dnsv4_addr;
              ppUdpHdr->DstPort = dnsv4_port;
            } else if (packet_v6) {
              ipv6_copy_addr(ppIpV6Hdr->DstAddr,
                             (uint32_t *)dnsv6_addr.s6_addr);
              ppUdpHdr->DstPort = dnsv6_port;
            }
            should_recalc_checksum = 1;
          } else {
            if (dns_is_dns_packet(packet_data, packet_dataLen, 1))
              should_reinject = 0;

            if (do_dns_verb && !should_reinject) {
              printf("[DNS] Error handling outgoing packet: srcport = %hu, "
                     "dstport = %hu\n",
                     ntohs(ppUdpHdr->SrcPort), ntohs(ppUdpHdr->DstPort));
            }
          }
        }
      }

      if (should_reinject) {
        // printf("Re-injecting!\n");
        if (should_recalc_checksum) {
          packets_circumvented++;
          WinDivertHelperCalcChecksums(packet, packetLen, &addr, (UINT64)0LL);
        }
        WinDivertSend(w_filter, packet, packetLen, NULL, &addr);
      }
    } else {
      // error, ignore
      if (!exiting)
        printf("Error receiving packet! (Code: %lu)\n", GetLastError());
      break;
    }
  }
  deinit_all();
  return 0;
}
