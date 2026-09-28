/* Host-only libnx boundary fixture. Never on the runtime include path. */
#ifndef BK_TEST_SWITCH_H
#define BK_TEST_SWITCH_H
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
typedef uint32_t u32;
typedef uint64_t u64;
typedef uint32_t Result;
#define R_SUCCEEDED(rc) ((rc) == 0)
typedef enum { PcmFormat_Invalid, PcmFormat_Int16 } PcmFormat;
typedef struct AudioOutBuffer {
  struct AudioOutBuffer *next;
  void *buffer;
  u64 buffer_size, data_size, data_offset;
} AudioOutBuffer;
Result audoutInitialize(void);
void audoutExit(void);
u32 audoutGetSampleRate(void);
u32 audoutGetChannelCount(void);
PcmFormat audoutGetPcmFormat(void);
Result audoutStartAudioOut(void);
Result audoutStopAudioOut(void);
Result audoutAppendAudioOutBuffer(AudioOutBuffer *);
Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer **, u32 *);
typedef pthread_mutex_t Mutex;
typedef struct {
  pthread_t native;
  void (*entry)(void *);
  void *context;
} Thread;
void mutexInit(Mutex *);
void mutexLock(Mutex *);
void mutexUnlock(Mutex *);
Result threadCreate(Thread *, void (*)(void *), void *, void *, size_t, int,
                    int);
Result threadStart(Thread *);
Result threadWaitForExit(Thread *);
Result threadClose(Thread *);
void svcSleepThread(int64_t ns);
void armDCacheFlush(void *, size_t);
#endif
