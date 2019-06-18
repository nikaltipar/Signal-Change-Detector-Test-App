/*! \file pace.c
 *  \brief Implements a multiple signal change detector
 *  \date 24-10-2015
 *  \author Nikolaos Altiparmakis
 *
 *  This code segment is part of a project for the course "Real Time and"
 *  Embedded Systems" taught in the Aristotle University of Thessaloniki.
 *  Its task is to implement a multiple signal change detector by using
 *  pthreads.
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

/*! \brief The number of threads to be used for the signal detectors.
*/
#define NUM_THREADS 2

/*! \brief Time before the signal to exit triggers.
*/
#define SEC_TO_EXIT 20

//! A struct holding the barriers for each thread's scope
/*! 
  This struct holds two integers, corresponding to the barriers, in which
  the respective signal change detector must look for signal changes. For each
  thread instance, those are calculated withing main.
*/
struct barrier
{
  //! The starting point of a detector's area of interest.
  int start;
  //! The finishing point of a detector's area of interest.
  int end;
};

//! The signals' current value.
volatile int* signalArray;
//! Used to inform whether a change from 0 to 1 has been detected.
int* notDetectedArray;
//! A pointer of threads used for the signal detectors
pthread_t* sigDet;
//! A struct holding time stamps on signal changes and detections.
struct timeval* timeStamp;
//! Number of signals
int N;
//! Number of detected 0->1 changes
int detected;
//! Number of late detected 0->1 changes
int misses;
//! Number of 0->1 changes
int changed;
//! A thread attribute object used to make the detector threads joinable.
pthread_attr_t attrDet;
//! A mutex used for safely updating the info on detected and missed singals.
pthread_mutex_t detectorLock;

/*!
 * \brief Terminates the program.
 *
 * After a SIGALRM signal triggers through the set timer, this function frees 
 * all allocated memory space and terminates the program.
 *
 * \param[in] sig
 * \return void
 */

void exitfunc(int sig)
{
  printf(
        "-----------\nChanged: %d \nDetected: %d \nMisses: %d\nSuccess: %.2f\n",
        changed,
        detected,
        misses,
        ((double)detected - misses)/changed * 100);

  free(sigDet);
  free(timeStamp);
  free(signalArray);
  free(notDetectedArray);
  pthread_mutex_destroy(&detectorLock);
  pthread_attr_destroy(&attrDet);

  _exit(0);
}


/*!
 * \brief Randomly changes a signal's value.
 *
 * This functions runs indefinitely and changes a randomly selected signal's
 * value from 0 to 1 or vice-versa. The time in which a signal changes is
 * tracked and stored and the event is reported through STDOUT. A variable
 * is incremented, which stores the number of times a signal has been changed
 * from 0 to 1.
 *
 * \param[in] args 
 * \return void*
 * \note Implemented through pthreads.
 */

void* SensorSignalReader (void* args);


/*!
 * \brief Scans the matrix for signal changes and reports them.
 *
 * This code segment scans a certain amount of signals for changes. When one is
 * found, it's reported immediately through STDOUT, along with the time it took
 * to find that particular change. An ID number is passed through the
 * arguments, which is used to split  the matrix into segments, depending on 
 * the number of threads used for this task. Two variables get possibly 
 * incremented, the one of which stores the number of times a change from 0 to
 * 1 has happened, the other whether the the signal has been detected within
 * certain time constraints (worst case 1 us).
 *
 * \param[in] args 
 * \return void*
 * \note Implemented through pthreads.
 */

void* ChangeDetector (void* args);



/*!
 * \brief Defines pthreads, allocates memory space and waits for SIGALRM
 *
 * This code segment contains only the pre-processing steps. It sets the 
 * time to trigger the exit alarm, allocates space for the pthreads and 
 * the signals' needed space, creates the needed pthreads and waits 
 * indefinitely through pthread_join.
 *
 *
 * \param[in] argc 
 * \param[in] argv
 * \return int
 * \note Requires the number of signals through the console.
 */

int main(int argc, char** argv)
{
  int i;
  pthread_t sigGen;
  struct barrier barriers[NUM_THREADS];
  N = atoi(argv[1]);

  
  if (argc != 2) 
  {
    printf("Usage: %s N\n"
           " where\n"
           " N    : number of signals to monitor\n"
	   , argv[0]);
    
    return (1);
  }
  
  for (i = 0; i < NUM_THREADS; i++)
  {
    int segmentSize = ceil(((double) N) / NUM_THREADS);
    barriers[i].start = i * segmentSize;

    switch (i)
    {
      case NUM_THREADS - 1:
        barriers[i].end = N;
        //-----
        break;

      default:
        barriers[i].end = barriers[i].start + segmentSize;
        //-----
        break;
    }

  }
  pthread_attr_init(&attrDet);
  pthread_attr_setdetachstate(&attrDet, PTHREAD_CREATE_JOINABLE);
  pthread_mutex_init(&detectorLock, NULL);

  sigDet = (pthread_t*) malloc(NUM_THREADS * sizeof(pthread_t));
  if (sigDet == NULL)
  {
    exit(-1);
  }

  signal(SIGALRM, exitfunc);
  alarm(SEC_TO_EXIT); 
  signal(SIGINT, exitfunc);
  signalArray = (int*) malloc(N * sizeof(int));
  if (signalArray == NULL)
  {
    exit(-1);
  }
  for (i = 0; i < N; i++) {
    signalArray[i] = 0;
  }

  notDetectedArray = (int*) malloc(N * sizeof(int));
  if (notDetectedArray == NULL)
  {
  	exit(-1);
  }
  for (i = 0; i < N; i++)
  {
  	notDetectedArray[i] == 1;
  }


  timeStamp = (struct timeval*) malloc(N * sizeof(struct timeval));
  if (timeStamp == NULL)
  {
    exit(-1);
  }


  for (i = 0; i < NUM_THREADS; i++)
  {
    pthread_create(&sigDet[i], &attrDet, (void*) ChangeDetector, 
      (void*) &barriers[i]);
  }

  pthread_create(&sigGen, NULL, SensorSignalReader, NULL);
  

  for (i = 0; i < NUM_THREADS; i++)
  {
    pthread_join(sigDet[i], NULL);
  }

  return 0;
}


void* SensorSignalReader (void* arg)
{

  char buffer[30];
  struct timeval tv; 
  time_t curtime;

  srand(time(NULL));

  while (1) 
  {
    int t = rand() % 10 + 1;
    usleep(t * 100000); 

    int r = rand() % N;
    

    signalArray[r] ^= 1;
    if (signalArray[r]) 
    {
      gettimeofday(&tv, NULL);
      changed++;
      timeStamp[r] = tv;
    }
  }
}

void* ChangeDetector (void *arg)
{
  char buffer[30];
  struct timeval tv; 
  time_t curtime;

  struct barrier barriers = *((struct barrier*) arg);

	int j = barriers.start;

  while (1) 
  {
    
    if (signalArray[j] * notDetectedArray[j] == 1)
    {
    	gettimeofday(&tv, NULL); 
    	notDetectedArray[j] = 0;
      curtime = tv.tv_sec;
      strftime(buffer, 30, "%d-%m-%Y  %T.", localtime(&curtime));
      

        pthread_mutex_lock(&detectorLock);
        detected++;
        if (tv.tv_sec - timeStamp[j].tv_sec > 0 || 
                    tv.tv_usec - timeStamp[j].tv_usec > 1)
        {
          misses++;
          printf(
            "Detcted %5d at Time %s%ld after %ld.%06ld sec\n",
            j,
            buffer,
            tv.tv_usec,
            tv.tv_sec - timeStamp[j].tv_sec,
            tv.tv_usec - timeStamp[j].tv_usec
            );
        }
        pthread_mutex_unlock(&detectorLock);
    }

    if (!(signalArray[j] || notDetectedArray[j]))
    {
    	notDetectedArray[j] = 1;
    }

    j = (j  >= barriers.end - 1) ? barriers.start : j + 1;
  }
}
