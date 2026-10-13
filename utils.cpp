#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <math.h>
#include <sys/file.h>

#include "utils.h"

bool utils_runCmd(const char *argv[])
{
    pid_t pid = fork();
    if (pid == 0) {
        execvp(argv[0], (char* const*)argv);
        _exit(127);
    } else if (pid > 0) {
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            return false;
        }
        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
    return false;
}

void utils_copyString(char *dst, const char *src, unsigned long dst_size)
{
    if (dst_size <= strlen(src)) {
        fprintf(stderr, "Source path is longer than buffer, the path will be drop, src size = %ld, dst size = %ld !!!\n", strlen(src), dst_size);
    }

    strncpy(dst, src, dst_size);
    dst[dst_size - 1] = '\0';
}

void utils_timer_init(tTimer *ptTimer)
{
    ptTimer->existed = false;
    pthread_mutex_init(&ptTimer->mutex, NULL);
}

void utils_timer_uninit(tTimer *ptTimer)
{
    utils_timer_stop(ptTimer);
    
    pthread_mutex_destroy(&ptTimer->mutex);
}

static void *utils_timer_wait(void *user_data)
{
    tTimer *ptTimer = (tTimer *)user_data;

    pthread_mutex_lock(&ptTimer->mutex);
    ptTimer->existed = true;
    unsigned int seconds = ptTimer->seconds;
    pthread_mutex_unlock(&ptTimer->mutex);

    sleep(seconds);
    
    pthread_mutex_lock(&ptTimer->mutex);
    ptTimer->callback(ptTimer->user_data);
    pthread_mutex_unlock(&ptTimer->mutex);

    return NULL;
}

void utils_timer_start(tTimer *ptTimer, unsigned int seconds, TimerCallback callback, void *user_data)
{
    utils_timer_stop(ptTimer);

    ptTimer->seconds = seconds;
    ptTimer->callback = callback;
    ptTimer->user_data = user_data;

    if (pthread_create(&ptTimer->thread, NULL, utils_timer_wait, ptTimer) != 0) {
        return ;
    }
}

void utils_timer_stop(tTimer *ptTimer)
{
    pthread_mutex_lock(&ptTimer->mutex);
    bool existed = ptTimer->existed;
    pthread_mutex_unlock(&ptTimer->mutex);
    
    if (existed)
    {
        pthread_cancel(ptTimer->thread);
        pthread_join(ptTimer->thread, NULL);
        ptTimer->existed = false;
    }
}

int utils_ensureSingleInstance(const char *lockPath)
{
    int fd = open(lockPath, O_CREAT | O_RDWR, 0644);
    if (fd < 0) {
        fprintf(stderr, "open lock file\n");
        return -1;
    }

    if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
        fprintf(stderr, "flock\n");
        close(fd);
        return 0;
    }

    ftruncate(fd, 0);
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%ld\n", (long)getpid());
    if (len > 0) {
        (void)write(fd, buf, (size_t)len);
    }

    return 1;
}
