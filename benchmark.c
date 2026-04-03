/*
 * System Benchmark Tool
 *
 * Run: ./benchmark  (or benchmark.exe on Windows)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef _OPENMP
#include <omp.h>
#endif

/* ── Platform-specific includes ────────────────────────────────────── */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define POPEN  _popen
#define PCLOSE _pclose
#define DEVNULL "NUL"
#else
#include <unistd.h>
#include <sys/sysinfo.h>
#define POPEN  popen
#define PCLOSE pclose
#define DEVNULL "/dev/null"
#endif

/* ── Portable high-resolution timer ────────────────────────────────── */
#ifdef _WIN32
static double get_time_sec(void) {
    LARGE_INTEGER freq, cnt;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&cnt);
    return (double)cnt.QuadPart / (double)freq.QuadPart;
}
#else
#include <sys/time.h>
static double get_time_sec(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}
#endif

/* ── Helpers ───────────────────────────────────────────────────────── */
static void trim(char *s) {
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == '\n' || *end == '\r' || *end == ' ')) *end-- = '\0';
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

static void print_separator(void) {
    printf("────────────────────────────────────────────────────────\n");
}

/* ── System information ────────────────────────────────────────────── */
static void print_system_info(int *out_cores, char *out_cpu, size_t cpu_len) {
    printf("\n");
    print_separator();
    printf("  SYSTEM INFORMATION\n");
    print_separator();

#ifdef _WIN32
    /* CPU name from registry */
    {
        char cpu_name[256] = "Unknown";
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD size = sizeof(cpu_name);
            RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL,
                             (LPBYTE)cpu_name, &size);
            RegCloseKey(hKey);
        }
        trim(cpu_name);
        printf("  CPU           : %s\n", cpu_name);
        if (out_cpu) { strncpy(out_cpu, cpu_name, cpu_len - 1); out_cpu[cpu_len - 1] = '\0'; }
    }
    /* Cores / threads */
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        int logical = (int)si.dwNumberOfProcessors;
        *out_cores = logical;
        printf("  Logical cores : %d\n", logical);
    }
    /* CPU frequency (approximate from registry) */
    {
        HKEY hKey;
        DWORD mhz = 0, size = sizeof(mhz);
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            RegQueryValueExA(hKey, "~MHz", NULL, NULL, (LPBYTE)&mhz, &size);
            RegCloseKey(hKey);
        }
        if (mhz > 0)
            printf("  Base clock    : %lu MHz\n", (unsigned long)mhz);
    }
    /* Memory */
    {
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatusEx(&ms);
        printf("  Total RAM     : %.1f GB\n",
               (double)ms.ullTotalPhys / (1024.0 * 1024.0 * 1024.0));
        printf("  Available RAM : %.1f GB\n",
               (double)ms.ullAvailPhys / (1024.0 * 1024.0 * 1024.0));
    }
#else
    /* Linux: read /proc/cpuinfo */
    {
        FILE *f = fopen("/proc/cpuinfo", "r");
        char line[512];
        char cpu_name[256] = "Unknown";
        double mhz = 0;
        int cores = 0;
        if (f) {
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "model name", 10) == 0 && cpu_name[0] == 'U') {
                    char *p = strchr(line, ':');
                    if (p) { strncpy(cpu_name, p + 2, sizeof(cpu_name) - 1); trim(cpu_name); }
                }
                if (strncmp(line, "cpu MHz", 7) == 0 && mhz == 0) {
                    char *p = strchr(line, ':');
                    if (p) mhz = atof(p + 2);
                }
                if (strncmp(line, "processor", 9) == 0) cores++;
            }
            fclose(f);
        }
        *out_cores = cores > 0 ? cores : 1;
        printf("  CPU           : %s\n", cpu_name);
        if (out_cpu) { strncpy(out_cpu, cpu_name, cpu_len - 1); out_cpu[cpu_len - 1] = '\0'; }
        printf("  Logical cores : %d\n", *out_cores);
        if (mhz > 0) printf("  Base clock    : %.0f MHz\n", mhz);
    }
    /* Memory */
    {
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            double total = (double)si.totalram * si.mem_unit;
            double avail = (double)si.freeram * si.mem_unit;
            printf("  Total RAM     : %.1f GB\n", total / (1024.0*1024.0*1024.0));
            printf("  Available RAM : %.1f GB\n", avail / (1024.0*1024.0*1024.0));
        }
    }
#endif

#ifdef _OPENMP
    printf("  OpenMP        : yes (%d threads)\n", omp_get_max_threads());
#else
    printf("  OpenMP        : no (single-threaded)\n");
#endif

    print_separator();
    printf("\n");
}

/* ── Benchmark 1: Integer arithmetic (prime sieve) ─────────────────── */
static double bench_integer(void) {
    const int N = 20000000;   /* sieve up to 20 million */
    unsigned char *sieve = (unsigned char *)calloc(N + 1, 1);
    if (!sieve) return 0;

    double t0 = get_time_sec();

    for (int i = 2; (long long)i * i <= N; i++) {
        if (!sieve[i]) {
            for (int j = i * i; j <= N; j += i)
                sieve[j] = 1;
        }
    }

    /* count primes to prevent optimisation */
    volatile int count = 0;
    for (int i = 2; i <= N; i++)
        if (!sieve[i]) count++;

    double elapsed = get_time_sec() - t0;
    free(sieve);
    printf("    Primes found : %d\n", (int)count);
    return elapsed;
}

/* ── Benchmark 2: Floating-point (compute-heavy math) ──────────────── */
static double bench_float(void) {
    const long long ITERS = 80000000LL;
    volatile double result = 0.0;

    double t0 = get_time_sec();
    double acc = 0.0;
    for (long long i = 1; i <= ITERS; i++) {
        double x = (double)i * 0.0000001;
        acc += sin(x) * cos(x) + sqrt(x);
    }
    result = acc;
    double elapsed = get_time_sec() - t0;
    printf("    Checksum     : %.6f\n", (double)result);
    return elapsed;
}

/* ── Benchmark 3: Floating-point multi-threaded ────────────────────── */
static double bench_float_mt(void) {
    const long long ITERS = 80000000LL;
    double total = 0.0;

    double t0 = get_time_sec();

#ifdef _OPENMP
    #pragma omp parallel reduction(+:total)
    {
        long long chunk = ITERS / omp_get_num_threads();
        long long start = (long long)omp_get_thread_num() * chunk + 1;
        long long end   = (omp_get_thread_num() == omp_get_num_threads()-1) ? ITERS : start + chunk - 1;
        double acc = 0.0;
        for (long long i = start; i <= end; i++) {
            double x = (double)i * 0.0000001;
            acc += sin(x) * cos(x) + sqrt(x);
        }
        total += acc;
    }
#else
    for (long long i = 1; i <= ITERS; i++) {
        double x = (double)i * 0.0000001;
        total += sin(x) * cos(x) + sqrt(x);
    }
#endif

    double elapsed = get_time_sec() - t0;
    printf("    Checksum     : %.6f\n", total);
    return elapsed;
}

/* ── Benchmark 4: Memory bandwidth ─────────────────────────────────── */
static double bench_memory(void) {
    const size_t SIZE = 128 * 1024 * 1024; /* 128 MB */
    const int PASSES = 5;
    long long *buf = (long long *)malloc(SIZE);
    if (!buf) return 0;
    size_t count = SIZE / sizeof(long long);

    /* fill */
    for (size_t i = 0; i < count; i++) buf[i] = (long long)i;

    double t0 = get_time_sec();

    for (int p = 0; p < PASSES; p++) {
        volatile long long sum = 0;
        for (size_t i = 0; i < count; i++)
            sum += buf[i];
    }

    double elapsed = get_time_sec() - t0;
    double gb = (double)SIZE * PASSES / (1024.0 * 1024.0 * 1024.0);
    printf("    Throughput   : %.2f GB/s\n", gb / elapsed);

    free(buf);
    return elapsed;
}

/* ── Scoring ───────────────────────────────────────────────────────── */
/*
 * Reference times (seconds) calibrated to a "1000-point" baseline.
 * A faster machine gets a higher score.
 */
static const double REF_INTEGER  = 0.35;
static const double REF_FLOAT_ST = 2.50;
static const double REF_FLOAT_MT = 0.60;
static const double REF_MEMORY   = 1.50;

static double score(double ref, double actual) {
    if (actual <= 0) return 0;
    return (ref / actual) * 1000.0;
}

/* ── Submit results to leaderboard ─────────────────────────────────── */
static void submit_results(const char *cpu_name, const char *computername,
                           int cores, double s_int, double s_fst,
                           double s_fmt, double s_mem, double overall) {
    const char *url = "https://unesonqawvpgigjudvnm.supabase.co/rest/v1/results";
    const char *apikey = "sb_publishable_obUFweX2kWRDofOLvV7jYg_j376QTL9";

#ifdef _WIN32
    const char *os_name = "Windows";
#elif defined(__APPLE__)
    const char *os_name = "macOS";
#else
    const char *os_name = "Linux";
#endif

    /* Sanitise strings for JSON (strip quotes and backslashes) */
    char safe_cpu[256], safe_host[256];
    const char *srcs[] = { cpu_name, computername };
    char *dsts[] = { safe_cpu, safe_host };
    for (int k = 0; k < 2; k++) {
        const char *s = srcs[k];
        char *d = dsts[k];
        char *end = d + 255;
        while (*s && d < end) {
            if (*s != '"' && *s != '\\') *d++ = *s;
            s++;
        }
        *d = '\0';
    }

    /* Write JSON payload to a temp file (avoids shell-quoting issues) */
    const char *tmpf = "bench_submit.json";
    FILE *f = fopen(tmpf, "w");
    if (!f) { printf("  Could not create temp file.\n"); return; }
    fprintf(f,
        "{\"cpu_name\":\"%s\",\"computername\":\"%s\",\"cores\":%d,\"os\":\"%s\","
        "\"score_int\":%.1f,\"score_float_st\":%.1f,"
        "\"score_float_mt\":%.1f,\"score_mem\":%.1f,"
        "\"score_overall\":%.1f}",
        safe_cpu, safe_host, cores, os_name, s_int, s_fst, s_fmt, s_mem, overall);
    fclose(f);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "curl -s -o " DEVNULL " -w \"%%{http_code}\" "
        "-X POST \"%s\" "
        "-H \"apikey: %s\" "
        "-H \"Authorization: Bearer %s\" "
        "-H \"Content-Type: application/json\" "
        "-H \"Prefer: return=minimal\" "
        "--data @%s",
        url, apikey, apikey, tmpf);

    printf("  Submitting...\n");
    FILE *p = POPEN(cmd, "r");
    if (p) {
        char status[16] = {0};
        if (fgets(status, sizeof(status), p))
            trim(status);
        PCLOSE(p);
        if (strncmp(status, "201", 3) == 0) {
            printf("  Submitted successfully!\n");
        } else {
            printf("  Submission failed (HTTP %s).\n", status);
        }
    } else {
        printf("  Could not run curl. Is it installed?\n");
    }

    remove(tmpf);
}

/* ── Main ──────────────────────────────────────────────────────────── */
int main(void) {
    int cores = 1;
    char cpu_name[256] = "Unknown";
    char computername[256] = "Unknown";
    print_system_info(&cores, cpu_name, sizeof(cpu_name));

#ifdef _WIN32
    { DWORD size = sizeof(computername); GetComputerNameA(computername, &size); }
#else
    gethostname(computername, sizeof(computername));
#endif

    /* Only reveal name for school machines (pattern: fXrYsZ) */
    {
        const char *h = computername;
        int is_school = (h[0] == 'f' && h[1] >= '0' && h[1] <= '9'
                      && h[2] == 'r' && h[3] >= '0' && h[3] <= '9'
                      && h[4] == 's' && h[5] >= '0' && h[5] <= '9');
        if (!is_school) strcpy(computername, "\xe2\x80\x94"); /* em dash */
    }
    printf("  Computer      : %s\n\n", computername);

    printf("  Running benchmarks … this may take a minute.\n\n");

    /* 1 - Integer */
    printf("  [1/4] Integer (prime sieve, single-thread)\n");
    double t_int = bench_integer();
    double s_int = score(REF_INTEGER, t_int);
    printf("    Time         : %.3f s\n", t_int);
    printf("    Score        : %.0f\n\n", s_int);

    /* 2 - Float single-thread */
    printf("  [2/4] Floating-point (single-thread)\n");
    double t_fst = bench_float();
    double s_fst = score(REF_FLOAT_ST, t_fst);
    printf("    Time         : %.3f s\n", t_fst);
    printf("    Score        : %.0f\n\n", s_fst);

    /* 3 - Float multi-thread */
    printf("  [3/4] Floating-point (multi-thread)\n");
    double t_fmt = bench_float_mt();
    double s_fmt = score(REF_FLOAT_MT, t_fmt);
    printf("    Time         : %.3f s\n", t_fmt);
    printf("    Score        : %.0f\n\n", s_fmt);

    /* 4 - Memory */
    printf("  [4/4] Memory bandwidth (128 MB × 5 passes)\n");
    double t_mem = bench_memory();
    double s_mem = score(REF_MEMORY, t_mem);
    printf("    Time         : %.3f s\n", t_mem);
    printf("    Score        : %.0f\n\n", s_mem);

    /* Overall */
    double overall = (s_int * 0.20) + (s_fst * 0.25) + (s_fmt * 0.30) + (s_mem * 0.25);

    print_separator();
    printf("  RESULTS SUMMARY\n");
    print_separator();
    printf("  Integer  (20%%)      : %8.0f\n", s_int);
    printf("  Float ST (25%%)      : %8.0f\n", s_fst);
    printf("  Float MT (30%%)      : %8.0f\n", s_fmt);
    printf("  Memory   (25%%)      : %8.0f\n", s_mem);
    print_separator();
    printf("  OVERALL SCORE        : %8.0f\n", overall);
    print_separator();
    printf("\n  Higher is better. 1000 = reference baseline.\n");
    printf("  Compare this number across machines.\n\n");

    /* Ask to submit */
    printf("  Submit results to online leaderboard? [y/N] ");
    fflush(stdout);
    int ch = getchar();
    if (ch == 'y' || ch == 'Y') {
        submit_results(cpu_name, computername, cores, s_int, s_fst, s_fmt, s_mem, overall);
    }
    printf("\n");

    return 0;
}
