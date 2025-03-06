#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif

pid_t wait (int *);
pid_t waitpid (pid_t, int *, int );

#ifdef __cplusplus
}
#endif
