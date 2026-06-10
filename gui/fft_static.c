#include <pthread.h>
pthread_mutex_t x42_meters_fftw_planner_lock = PTHREAD_MUTEX_INITIALIZER;
unsigned int    x42_meters_instance_count    = 0;
