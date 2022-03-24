/*
   UTSAV KC
   1001733635
*/

#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdbool.h>

/*** Constants that define parameters of the simulation ***/

#define MAX_SEATS 3        /* Number of seats in the professor's office */
#define professor_LIMIT 10 /* Number of students the professor can help before he needs a break */
#define MAX_STUDENTS 1000  /* Maximum number of students in the simulation */
#define MAX_CONSECUTIVE_CLASS 5      /*Maximum number of students from class A & B who can enter the office*/

#define CLASSA 0
#define CLASSB 1

/*
  Lock and unlock the critical region.
*/
pthread_mutex_t mutex_A;
pthread_cond_t prof_inside; //condition or professor outside on a break
pthread_cond_t student_out; //Condition Variable that signals if the student is out

static int students_in_office; /* Total numbers of students currently in the office */
static int classa_inoffice;    /* Total numbers of students from class A currently in the office */
static int classb_inoffice;    /* Total numbers of students from class B in the office */

static int students_since_break = 0; /*students after the break*/
static int prof_office;              /*professor if not inside the office*/
static int Cons_Student_A;              //consecutive students from class A
static int Cons_Student_B;              //consecutive students from class B
static int class_A_waiting;              //students from class A waiting outside
static int class_B_waiting;              //students from class B waiting outside

bool prof_not_arrive_office(); //Function to check professor is in office or not
bool condition_for_A();        //Function to check if the student from A can enter or not
bool condition_for_B();        //Function to check if the student from B can enter or not

typedef struct
{
  int arrival_time;  // time between the arrival of this student and the previous student
  int question_time; // time the student needs to spend with the professor
  int student_id;
  int class;
} student_info;

/* Called at beginning of simulation.  
 * variables and other global variables that you add.
 */
static int initialize(student_info *si, char *filename)
{
  students_in_office = 0;
  classa_inoffice = 0;
  classb_inoffice = 0;
  students_since_break = 0;
  prof_office = 0;
  Cons_Student_A = 0;
  Cons_Student_B = 0;
  class_A_waiting = 0;
  class_B_waiting = 0;

  pthread_cond_init(&prof_inside, NULL);
  pthread_cond_init(&student_out, NULL);
  pthread_mutex_init(&mutex_A, NULL);

  /* Read in the data file and initialize the student array */
  FILE *fp;

  if ((fp = fopen(filename, "r")) == NULL)
  {
    printf("Cannot open input file %s for reading.\n", filename);
    exit(1);
  }

  int i = 0;
  while ((fscanf(fp, "%d%d%d\n", &(si[i].class), &(si[i].arrival_time),
                 &(si[i].question_time)) != EOF) &&
         i < MAX_STUDENTS)
  {
    i++;
  }

  fclose(fp);
  return i;
}

/* Code executed by professor to simulate taking a break 
 * You do not need to add anything here.  
 */
static void take_break()
{
  printf("The professor is taking a break now.\n");
  sleep(5);
  assert(students_in_office == 0);
  students_since_break = 0;
}

/* Code for the professor thread.This is fully implemented except for synchronization
 * with the students.
 */
void *professorthread(void *junk)
{
  printf("The professor arrived and is starting his office hours\n");

  /* Loop while waiting for students to arrive. */
  while(1)
  {
    //locked access 
    pthread_mutex_lock(&mutex_A);

    //check if professor is in
    if(prof_not_arrive_office())
    {
      prof_office = 1;
      // professor not in office
      pthread_cond_broadcast(&prof_inside);
    }

    //Professor takes a break after 10 students
    if ((prof_not_arrive_office() || students_since_break == professor_LIMIT) &&
        students_in_office == 0)
    {
      take_break();
      /*Professor is in break.*/
      pthread_cond_broadcast(&prof_inside);
    }

    //End of critical region
    //Unlock access to shared variables
    pthread_mutex_unlock(&mutex_A);
  }
  pthread_exit(NULL);
}

/* Code executed by a class A student to enter the office.
 * You have to implement this.  Do not delete the assert() statements,
 * but feel free to add your own.
 */
void classa_enter()
{
  //locked access to variables
  pthread_mutex_lock(&mutex_A);

  class_A_waiting += 1;

  while(true)
  {
    //Students wait until professor is available
    while(prof_not_arrive_office() || students_since_break >= professor_LIMIT)
    {
      pthread_cond_wait(&prof_inside, &mutex_A);
    }

    //Students wait until they are allowed to enter
    while(condition_for_A())
    {
      pthread_cond_wait(&student_out, &mutex_A);
    }
    
    //otherwise they can enter the office
    if(!((prof_not_arrive_office() || students_since_break >= professor_LIMIT
            ) ||
           (condition_for_A()))) break;
  }

  students_in_office += 1;
  students_since_break += 1;
  classa_inoffice += 1;
  Cons_Student_B = 0;
  Cons_Student_A += 1;
  class_A_waiting -= 1;

  //critical region ends
  //unlock access to shared variables
  pthread_mutex_unlock(&mutex_A);
}

/* Code executed by a class B student to enter the office.
 * You have to implement this.  Do not delete the assert() statements,
 * but feel free to add your own.
 */
void classb_enter()
{
  //locked access
  pthread_mutex_lock(&mutex_A);

  class_B_waiting += 1;//increase the queue for class B outside the office

  while(true)
  {
    //Student have to wait 
    while(prof_not_arrive_office() || students_since_break >= professor_LIMIT)
    {
      pthread_cond_wait(&prof_inside, &mutex_A);
    }

    //Students wait until they can enter
    while(condition_for_B())
    {
      pthread_cond_wait(&student_out, &mutex_A);
    }

    //otherwise they can enter the office
    if(!((prof_not_arrive_office() ||students_since_break >= professor_LIMIT
            ) ||
           (condition_for_B()))) break;
  }

  students_in_office += 1;
  students_since_break += 1;
  classb_inoffice += 1;
  Cons_Student_A = 0;
  Cons_Student_B += 1;
  class_B_waiting -= 1;

  //ending the critical region
  //unlocked access
  pthread_mutex_unlock(&mutex_A);
}

/* Code executed by a student to simulate the time he spends in the office asking questions
 * You do not need to add anything here.  
 */
static void ask_questions(int t)
{
  sleep(t);
}

/* Code executed by a class A student when leaving the office.
 * You need to implement this.  Do not delete the assert() statements.
 */
static void classa_leave()
{
  //critical region begins
  //locked access
  pthread_mutex_lock(&mutex_A);

  students_in_office -= 1;
  classa_inoffice -= 1;

  //student from class A go ouut
  pthread_cond_broadcast(&student_out);

  //End of critical region
  //Unlock access to shared variables
  pthread_mutex_unlock(&mutex_A);
}

/* Code executed by a class B student when leaving the office.
 * You need to implement this.  Do not delete the assert() statements,
 * but feel free to add as many of your own as you like.
 */
static void classb_leave()
{
  //critical region begins
  //Locked access 
  pthread_mutex_lock(&mutex_A);

  students_in_office -= 1;
  classb_inoffice -= 1;

  //Student from B go out
  pthread_cond_broadcast(&student_out);

  //End of critical region
  //Unlock access to shared variables
  pthread_mutex_unlock(&mutex_A);
}

/* Main code for class A student threads.  
 * You do not need to change anything here, but you can add
 * debug statements to help you during development/debugging.
 */
void *classa_student(void *si)
{
  student_info *s_info = (student_info *)si;

  /* enter office */
  classa_enter();

  printf("Student %d from class A enters the office\n", s_info->student_id);

  assert(students_in_office <= MAX_SEATS && students_in_office >= 0);
  assert(classa_inoffice >= 0 && classa_inoffice <= MAX_SEATS);
  assert(classb_inoffice >= 0 && classb_inoffice <= MAX_SEATS);
  assert(classb_inoffice == 0);

  /* ask questions  --- do not make changes to the 3 lines below*/
  printf("Student %d from class A starts asking questions for %d minutes\n", 
  s_info->student_id, s_info->question_time);
  ask_questions(s_info->question_time);
  printf("Student %d from class A finishes asking questions and prepares to leave\n", 
  s_info->student_id);

  /* leave office */
  classa_leave();

  printf("Student %d from class A leaves the office\n", s_info->student_id);

  assert(students_in_office <= MAX_SEATS && students_in_office >= 0);
  assert(classb_inoffice >= 0 && classb_inoffice <= MAX_SEATS);
  assert(classa_inoffice >= 0 && classa_inoffice <= MAX_SEATS);

  pthread_exit(NULL);
}

/* Main code for class B student threads.
 * You do not need to change anything here, but you can add
 * debug statements to help you during development/debugging.
 */
void *classb_student(void *si)
{
  student_info *s_info = (student_info *)si;

  /* enter office */
  classb_enter();

  printf("Student %d from class B enters the office\n", s_info->student_id);

  assert(students_in_office <= MAX_SEATS && students_in_office >= 0);
  assert(classb_inoffice >= 0 && classb_inoffice <= MAX_SEATS);
  assert(classa_inoffice >= 0 && classa_inoffice <= MAX_SEATS);
  assert(classa_inoffice == 0);

  printf("Student %d from class B starts asking questions for %d minutes\n", s_info->student_id,
   s_info->question_time);
  ask_questions(s_info->question_time);
  printf("Student %d from class B finishes asking questions and prepares to leave\n",
   s_info->student_id);

  /* leave office */
  classb_leave();

  printf("Student %d from class B leaves the office\n", s_info->student_id);

  assert(students_in_office <= MAX_SEATS && students_in_office >= 0);
  assert(classb_inoffice >= 0 && classb_inoffice <= MAX_SEATS);
  assert(classa_inoffice >= 0 && classa_inoffice <= MAX_SEATS);

  pthread_exit(NULL);
}

/* Main function sets up simulation and prints report
 * at the end.
 * GUID: 355F4066-DA3E-4F74-9656-EF8097FBC985
 */
int main(int nargs, char **args)
{
  int i;
  int result;
  int student_type;
  int num_students;
  void *status;
  pthread_t professor_tid;
  pthread_t student_tid[MAX_STUDENTS];
  student_info s_info[MAX_STUDENTS];

  if (nargs != 2)
  {
    printf("Usage: officehour <name of inputfile>\n");
    return EINVAL;
  }

  num_students = initialize(s_info, args[1]);
  if (num_students > MAX_STUDENTS || num_students <= 0)
  {
    printf("Error:  Bad number of student threads. "
           "Maybe there was a problem with your input file?\n");
    return 1;
  }

  printf("Starting officehour simulation with %d students ...\n",
         num_students);

  result = pthread_create(&professor_tid, NULL, professorthread, NULL);

  if (result)
  {
    printf("officehour:  pthread_create failed for professor: %s\n", strerror(result));
    exit(1);
  }

  for (i = 0; i < num_students; i++)
  {

    s_info[i].student_id = i;
    sleep(s_info[i].arrival_time);

    student_type = random() % 2;

    if (s_info[i].class == CLASSA)
    {
      result = pthread_create(&student_tid[i], NULL, classa_student, (void *)&s_info[i]);
    }
    else // student_type == CLASSB
    {
      result = pthread_create(&student_tid[i], NULL, classb_student, (void *)&s_info[i]);
    }

    if (result)
    {
      printf("officehour: thread_fork failed for student %d: %s\n",
             i, strerror(result));
      exit(1);
    }
  }

  /* wait for all student threads to finish */
  for (i = 0; i < num_students; i++)
  {
    pthread_join(student_tid[i], &status);
  }

  /* tell the professor to finish. */
  pthread_cancel(professor_tid);

  printf("Office hour simulation done.\n");

  return 0;
}


//Function to check if professor has arrived or not//
bool prof_not_arrive_office()
{
  bool a;

  if (!prof_office)
  {
    a = true;
  }
  return a;
}

//Function to check for a student of class A to enter
bool condition_for_A()
{
  bool a;

  if (!((Cons_Student_A < MAX_CONSECUTIVE_CLASS || class_B_waiting == 0) &&
        (classb_inoffice == 0) &&
        (students_in_office < MAX_SEATS)))
  {
    a = true;
  }
  return a;
}

//Funtion to check for student of class B to enter 
bool condition_for_B()
{
  bool flag;

  if (!((Cons_Student_B < MAX_CONSECUTIVE_CLASS || class_A_waiting == 0) &&
        (classa_inoffice == 0) &&
        (students_in_office < MAX_SEATS)))
  {
    flag = true;
  }
  return flag;
}

