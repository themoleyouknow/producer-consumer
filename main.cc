/******************************************************************
 * The Main program with the two functions. A simple
 * example of creating and using a thread is provided.
 ******************************************************************/

#include "helper.h"

void *producer (void *id);
void *consumer (void *id);

// Parameter structs:
struct Prod_Params {
  // Semaphore Array:
  int sem_ids;
  // Producer ID:
  int id;
  // Producer Constraints:
  int queue_size;
  int num_jobs;
  // Next index to place item in:
  int index = 0;
  // Pointer to queue:
  int *queue;
};

struct Cons_Params {
  // Semaphore Array:
  int sem_ids;
  // Consumer ID:
  int id;
  // Consumer Constraints:
  int queue_size;
  // Next index to consume:
  int index = 0;
  // Pointer to queue:
  int *queue;
};

int main (int argc, char **argv)
{
  /* -------------------------------------------- */
  // Check inputs:

  // Check if correct number of inputs passed:
  if (argc != 5) {
    cerr << "Input error: Incorrect number of inputs passed via command line." << endl;
    cerr << "Expected 4 arguments, received: " << argc << "." << endl;
    cerr << "Expected format is: (Size of Queue), (Number of Jobs), (Number of Producers), (Number of Consumers)." << endl;
    exit(1);
  }
  
  // Initialise inputs array:
  int inputs[4]; // 0 = Queue Size / 1 = Num of Jobs / 2 = Num Prods / 3  = Num Cons
  
  // Check inputs are valid (i.e integers)
  for (int arg_count = 1; arg_count < argc; arg_count++) {
    // Assign output from check_arg to input array:
    inputs[arg_count-1] = check_arg(argv[arg_count]);
    // Check if output was invalid, and return an error/exit if true:
    if (inputs[arg_count-1] < 0) {
      cerr << "Input Error: " << argv[arg_count] << " passed as argument " << arg_count << "." << endl;
      exit(1);
    }
  }  
  /* -------------------------------------------- */
  // Set-up and initialise the required data structures, variables etc.

  // Declare queue:
  int queue[inputs[0]];
  // Declare thread id array:
  pthread_t prod_thread_ids[inputs[2]];
  pthread_t cons_thread_ids[inputs[3]];
   // Declare producer and consumer parameter structures::
  Prod_Params prod_params;
  Cons_Params cons_params;
  /* -------------------------------------------- */
  // Set-up and initialise semapores as necessary:

  // Declare and set up semaphores:
  // Generate semaphores:
  int sem = sem_create(SEM_KEY, 4);
  if (sem < 0) {
    cerr << "sem_create Error: sem_create unable to create semaphores." << endl;
    cerr << sem;
    exit(1);
  }
  // Initialise semaphores:
  // mutex - binary to ensure mutual exclusion of queue, initialised to 1.
  if (sem_init(sem, 0, 1) < 0) {
    cerr << "sem_init Error: sem_init unable to initialise mutex semaphore." << endl;
    exit(1);
  }
  // space - check if there's space in the list, initialised to N.
  if (sem_init(sem, 1, inputs[0]) < 0) {
    cerr << "sem_init Error: sem_init unable to initialise space semaphore." << endl;
    exit(1);
  }
  // item - check if there is an item in the list, initialised to 0.
  if (sem_init(sem, 2, 0) < 0) {
    cerr << "sem_init Error: sem_init unable to initialise item semaphore." << endl;
    exit(1);
  }
  // id - ensure that producers and consumers are assigned correct IDs, initialised to 0.
  if (sem_init(sem, 3, 0) < 0) {
    cerr << "sem_init Error: sem_init unable to initialise id semaphore." << endl;
    exit(1);
  }
  /* -------------------------------------------- */  
  // Create the required producers and consumers:
  
  // Initialise producer parameter values:
  prod_params.sem_ids = sem;
  prod_params.queue_size = inputs[0];
  prod_params.num_jobs = inputs[1];
  prod_params.queue = queue;
  // Initialise consumer parameter values
  cons_params.sem_ids = sem;
  cons_params.queue_size = inputs[0];
  cons_params.queue = queue;
  
  // Create producers:
  for (int count = 0; count < inputs[2]; count++) {
    // Increment ID using counter (starting from 0, so first is 1):
    prod_params.id = count + 1;
    // Create producer thread:
    if (pthread_create(&prod_thread_ids[count], NULL, producer, (void *) &prod_params)) {
      cerr << "pthread_create Error: Unable to complete creating producers " << prod_params.id << "." << endl;
      exit(1);
    }
    // Wait for producer to be initialised:
    sem_wait(sem, 3);
  }

  // Create consumers:
  for (int count = 0; count < inputs[3]; count++) {
    // Increment ID using counter (starting from 0, so first is 1):
    cons_params.id = count + 1;
    // Create consumer thread:
    if (pthread_create(&cons_thread_ids[count], NULL, consumer, (void *) &cons_params)) {
      cerr << "pthread_create Error: Unable to complete creating consumers " << cons_params.id << "." << endl;
      exit(1);
    }
    // Wait for producer to be initialised:
    sem_wait(sem, 3);
  }
  
  // Join producer threads:
  for (int count = 0; count < inputs[2]; count++) {
    pthread_join(prod_thread_ids[count], NULL);
  }
  for (int count = 0; count < inputs[3]; count++) {
    pthread_join(cons_thread_ids[count], NULL);
  }
  /* -------------------------------------------- */
  // Quit, but ensure process cleanup!
  
  sem_close(sem);
  return 0;
}

/* ------------------- producer/consumer functions ---------------- */
void *producer (void *parameters) 
{
  /* ---- START OF ID CRITICAL SECTION ---- */
  // Initialise preliminary parameters:
  Prod_Params *params = (Prod_Params*) parameters;
  int prod_id = params->id;
  /* ----   END OF ID CRITICAL SECTION ---- */
  // Release id semaphore:
  sem_signal(params->sem_ids, 3);
  // Complete parameter initialisation:
  int job_counter = 1;
  int produced_item;
  int production_time;
  // Begin production process:
  while (job_counter <= params->num_jobs) {
    // Generate random production item value, and production time:
    produced_item = (rand() % ITEM_MAX + 1);
    production_time = (rand() % PROD_MAX + 1);
    // Wait till finished producing:
    sleep(production_time);
    // Check space semaphore to ensure we haven't been waiting too long for a space in the queue:
    if (sem_timeout(params->sem_ids, 1)) {
      cerr << "Timeout: Producer(" << prod_id << ") has timed out on job number ";
      cerr << job_counter << ".\n";
      pthread_exit(0);
    }
    // Request access to queue array:
    sem_wait(params->sem_ids, 0);
    /* ---- START OF MUTEX CRITICAL SECTION ---- */
    params->queue[params->index] = produced_item;
    cerr << "Producer(" << prod_id << "): ";
    cerr << "Job id " << params->index + 1;
    cerr << " duration " << produced_item << ".\n";
    params->index = (params->index + 1) % params->queue_size;
    /* ----   END OF MUTEX CRITICAL SECTION ---- */
    // Release mutex semaphore:
    sem_signal(params->sem_ids, 0);
    // Signal that there's an item in the queue using item semaphore:
    sem_signal(params->sem_ids, 2);
    // Update job counter:
    job_counter++;
  }
  // Print producer completion message:
  cerr << "Producer(" + to_string(prod_id) + "): No more jobs to generate.\n";
  // Exit thread:
  pthread_exit(0);
}

void *consumer (void *parameters) 
{
  /* ---- START OF ID CRITICAL SECTION ---- */
  // Initialise preliminary parameters:
  Cons_Params *params = (Cons_Params*) parameters;
  int cons_id = params->id;
  /* ----   END OF ID CRITICAL SECTION ---- */
  // Release id semaphore:
  sem_signal(params->sem_ids, 3);
  int consumed_item;
  int job_id;
  // Begin consumption process:
  while (1) {
    // Check item semaphore to ensure we haven't been waiting too long for an item:
    if (sem_timeout(params->sem_ids, 2)) {break;}
    // Request access to queue array:
    sem_wait(params->sem_ids, 0);
    /* ---- START OF MUTEX CRITICAL SECTION ---- */
    job_id = params->index;
    consumed_item = params->queue[job_id];
    cerr << "Consumer(" << cons_id << "): ";
    cerr << "Job id " << params->index + 1;
    cerr << " executing sleep duration " << consumed_item << ".\n";
    params->index = (params->index + 1) % params->queue_size;
    /* ----   END OF MUTEX CRITICAL SECTION ---- */
    // Release mutex semaphore:
    sem_signal(params->sem_ids, 0);
    // Signal that there's a space in the queue using space semaphore:
    sem_signal(params->sem_ids, 1);
    // Begin consumption:
    sleep(consumed_item);
    // Signal consumption completion;
    cerr << "Consumer(" + to_string(cons_id) + "): Job id " + to_string(job_id + 1) + " completed.\n";
  } 
  // Print producer completion message:
  cerr << "Consumer(" + to_string(cons_id) + "): No more jobs left.\n";
  // Exit thread:
  pthread_exit(0);
}
