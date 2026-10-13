#ifndef UTILITIES_H
#define UTILITIES_H

#include <pthread.h>
#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef void (*TimerCallback)(void *user_data);
typedef struct timer_st
{
    pthread_t thread;
    pthread_mutex_t mutex;
    unsigned int seconds;
    TimerCallback callback;
    void *user_data;
    bool existed;
} tTimer;

bool utils_runCmd(const char *argv[]);
void utils_copyString(char *dst, const char *src, unsigned long dst_size);
void utils_timer_init(tTimer *ptTimer);
void utils_timer_uninit(tTimer *ptTimer);
void utils_timer_start(tTimer *ptTimer, unsigned int seconds, TimerCallback callback, void *user_data);
void utils_timer_stop(tTimer *ptTimer);

int utils_ensureSingleInstance(const char *lockPath);

#ifdef __cplusplus
}
#endif

#endif /* UTILITIES_H */
