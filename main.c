/*
Tam H. Le
11/3/2020
program_3
*/

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <time.h>

#define MAX_BG_P 100
#define MAX_LINE 2500

static int enableBG = 1; //background flag

//using struct to storea all different elements incluced in a command
struct input
{
  char *command;
  char *argc[513];
  char *inputFile;
  char *outputFile;
  int background;
  int input_redirection;
  int output_redirection;
};
/*
prototypes
*/
char *read_input(ssize_t *lines_size);
int execute_command(struct input *input_line, int *status, pid_t *open_processes);
struct input *parse(char *lines, ssize_t line_size);
int newProcess(struct input *input_line, pid_t *open_processes);
void sigintHandler(int sig_num);
void sigintTSTP(int sig_num);
char *changeSign(char *line);
int processStatus(pid_t pid_num);

int main()
{

  ssize_t line_size;
  char *line;
  int status = 0;
  pid_t childPID;
  pid_t open_processes[MAX_BG_P]; //keeping track of open processes

  signal(SIGINT, sigintHandler);
  signal(SIGTSTP, sigintTSTP);

  //one time insitialze to zero
  for (int i = 0; i < 100; i++)
  {
    open_processes[i] = 0;
  }

  while (1)
  {
    // doing this because the termination status does not print instantly
    struct timespec ts; //using sleeper to make sure status printed before parent
    ts.tv_sec = 0;
    ts.tv_nsec = 500;
    nanosleep(&ts, NULL); //waiting for parent
    for (int i = 0; i < MAX_BG_P; i++)
    {
      if (open_processes[i] > 0)
      {
        if (processStatus(open_processes[i]))
        {
          open_processes[i] = 0; //remove pid from array
        }
      }
    }
    printf(": "); //prompt
    fflush(stdout);
    line = read_input(&line_size);                                          //insitalize line and take in input
    line = changeSign(line);                                                //checking for $$
    struct input *input_line = parse(line, line_size);                      //parsing our input
    int return_code = execute_command(input_line, &status, open_processes); //return code to break
    if (return_code == 0)
    {
      break;
    }
  }

  return 0;
}

/*
function to determine background processes for termination or background exit status 
of the child. added WSTOPSIG just in case if child proces for some reason stopped other than signaled or exited
*/
int processStatus(pid_t pid_num)
{
  int status;
  pid_t childPID;
  childPID = waitpid(pid_num, &status, WNOHANG);
  if (childPID > 0)
  {

    if (WIFSIGNALED(status)) // check if child process is terminated
    {
      int exitStatus = WTERMSIG(status);
      printf("Background pid %d is done: terminated by signal %d\n", childPID, exitStatus);
      fflush(stdout);
    }
    else if (WIFEXITED(status)) // check if child exits normal
    {
      printf("\nBackground pid %d is done: exit value is %d\n", childPID, WEXITSTATUS(status));
    }
    else if (WIFSTOPPED(status)) // checks in case if something else stops child
    {
      int exitStatus = WSTOPSIG(status);
      printf("Background pid %d is done: terminated by signal %d\n", childPID, exitStatus);
      fflush(stdout);
    }
    else
    {
      return 0;
    }
    return 1;
  }
  return 0;
}

/*
converting $$ to pid  by looping through line for $ 
if first is found, move on to the next and if its $
then copy to var1
this was used because it was more straightfoward to check
for each index. 
*/
char *changeSign(char *line)
{

  char *var1 = (char *)malloc(MAX_LINE * sizeof(char)); //malloc mem to var1
  pid_t pid_value = getpid();                           // get current pid value
  char str_pid[10];                                     //set up pid storage as string
  int dest = 0;                                         // keep track keeping next character
  sprintf(str_pid, "%d", pid_value);                    //move pid to as str_pid
  for (int i = 0; i < strlen(line); i++)                //loop through through len of line
  {
    if (line[i] != '$')
    { //if not move on to next index
      var1[dest] = line[i];
      dest++;
    }
    else
    {
      if (i < strlen(line) - 1)
      { // if next index is not $ move to next
        if (line[i + 1] != '$')
        {
          var1[dest] = line[i];
          dest++;
        }
        else
        {
          for (int j = 0; j < strlen(str_pid); j++)
          {
            var1[dest] = str_pid[j];
            dest++;
          }
          i++;
        }
      }
      else
      {
        var1[dest] = line[i];
        dest++;
      }
    }
  }
  var1[dest] = '\0';
  free(line);
  return (var1);
}

void sigintChild(int sig_num)
{

  raise(SIGINT); // force a kill
  //exit(SIGINT);
}

//sig handler for SIGINT
void sigintHandler(int sig_num)
{
  /* Reset handler to catch SIGINT next time. 
       Refer http://en.cppreference.com/w/c/program/signal */
  signal(SIGINT, sigintHandler);
}

// for the child to ingore signal in foreground
void sigintChildTSTP(int sig_num)
{
  signal(SIGTSTP, sigintChildTSTP); //ignore and rest
}

// function to switch from foreground only mode to exiting it
void sigintTSTP(int sig_num)
{
  if (enableBG)
  {

    char *enter = ("\n Entering foreground-only mode(& is now ignored)\n");
    write(STDOUT_FILENO, enter, 50);
    fflush(stdout);
    enableBG = 0;
  }
  else
  {
    char *exiting = ("\nExiting foreground-only mode\n");
    write(STDOUT_FILENO, exiting, 30);
    fflush(stdout);
    enableBG = 1;
  }
  /* Reset handler to catch SIGINT next time. 
       Refer http://en.cppreference.com/w/c/program/signal */
  signal(SIGTSTP, sigintTSTP);
}

/*
parsing input from keyboard
the idea was taken from the first and secind project
and the parsing code was base on the module 1 examples 
*/
struct input *parse(char *line, ssize_t line_size)
{

  struct input *parsedLines = malloc(sizeof(struct input));
  int counter = 0;
  //searching for first instance of #
  for (int i = 0; i < line_size; i++) //looping through line to search for #
  {

    if (line[i] == '#')
    {
      line[i] = '\0';
      break;
    }
  }
  char *saveptr = line;

  //stripping blank spaces at the end
  // if any spaces before hashtag strip it.
  while (strlen(line) > 0 && (line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == ' '))
  {
    line[strlen(line) - 1] = '\0';
  }

  //setting input to NULL for blank lines
  if (strlen(line) == 0)
  {
    parsedLines->command = NULL;
    parsedLines->argc[0] = NULL;
    return parsedLines;
  }

  if (line[strlen(line) - 1] == '&') //checking iflast value if &
  {
    //run in the background
    parsedLines->background = 1;
    line[strlen(line) - 1] = '\0';
  }
  else
  {
    parsedLines->background = 0;
  }

  if (!enableBG) // if we are not in bg mode
  {
    parsedLines->background = 0;
  }

  //intialize both to 0
  parsedLines->input_redirection = 0;
  parsedLines->output_redirection = 0;

  // first token is command which is an int
  char *token = strtok_r(line, " ", &saveptr);
  parsedLines->command = calloc(strlen(token) + 1, sizeof(char));
  strcpy(parsedLines->command, token);
  int loca = 0;
  while (1)
  {
    token = strtok_r(NULL, " ", &saveptr);
    if (token == NULL)
    {
      break;
    }
    parsedLines->argc[loca] = calloc(strlen(token) + 1, sizeof(char));
    strcpy(parsedLines->argc[loca], token);
    loca++;
  }
  parsedLines->argc[loca] = NULL;

  // checking if input is in argc
  // the idea is to find the < or > and set the redirection flags
  // then we move back arguement to execute the files later
  while (parsedLines->argc[counter] != NULL)
  {
    if (strcmp(parsedLines->argc[counter], "<") == 0) //searching for <
    {
      parsedLines->input_redirection = 1;         //chainging input redirection flag
      if (parsedLines->argc[counter + 1] != NULL) //checking for file
      {
        parsedLines->inputFile = parsedLines->argc[counter + 1]; //set it to inputFile
        free(parsedLines->argc[counter]);
        counter = counter + 2; //so we can move back in the stack after setting our flags
        while (parsedLines->argc[counter] != NULL)
        {
          parsedLines->argc[counter - 2] = parsedLines->argc[counter];
          counter++;
        }
        parsedLines->argc[counter - 2] = NULL;
      }
      else
      {
        parsedLines->inputFile = NULL;
      }
    }
    counter++;
  }

  // checking if output is in argc
  //same while loop as input
  counter = 0;
  while (parsedLines->argc[counter] != NULL)
  {
    if (strcmp(parsedLines->argc[counter], ">") == 0)
    {
      parsedLines->output_redirection = 1;
      if (parsedLines->argc[counter + 1] != NULL)
      {
        parsedLines->outputFile = parsedLines->argc[counter + 1];
        free(parsedLines->argc[counter]);
        counter = counter + 2;
        while (parsedLines->argc[counter] != NULL)
        {
          parsedLines->argc[counter - 2] = parsedLines->argc[counter];
          counter++;
        }
        parsedLines->argc[counter - 2] = NULL;
      }
      else
      {
        parsedLines->outputFile = NULL;
      }
    }
    counter++;
  }

  free(line);
  return parsedLines;
}

//reading the input using getline
//getline was easier to use than scanf
char *read_input(ssize_t *lines_size)
{
  char *buffer = NULL;
  size_t len = 0;
  *lines_size = 0;
  *lines_size = getline(&buffer, &len, stdin);
  return buffer;
}

/*&placing input structure into an array to be used with execvp
this was used because execvp requires a command and array as arguements 
within this array, it also needed the command as well. 
*/
void argc_list(struct input *input_line, char **argsr)
{
  int i = 0;
  //set the command argument as the first value in our array
  argsr[0] = input_line->command;
  //
  while (input_line->argc[i] != NULL)
  {
    //after first index, populate the rest
    argsr[i + 1] = input_line->argc[i];
    i++;
  }
  //execvp needs NULL at the end, so this is necessary
  argsr[i + 1] = NULL;
}

/*
creating new process
this is where we fork 
create the child and use execvp
also make parent wait until termination status 
taken from Exploration: Process API
*/
int newProcess(struct input *input_line, pid_t *open_processes)
{
  char *argc[512];
  int inputFile_disc;
  int outputFile_disc;
  pid_t spawnpid = -5;
  pid_t wait_pid;
  spawnpid = fork();
  int status;

  if (spawnpid == 0)
  {
    // Child process

    signal(SIGTSTP, sigintChildTSTP);

    if (input_line->background) // if &
    {
      signal(SIGINT, sigintHandler);
      //  suppose to handle, but it caused more issues with it

      // if background is entered then redirect to /dev/null
      //if(!input_line->input_redirection)
      //{
      //sprintf(input_line->inputFile,"%s","/dev/null");
      //input_line->input_redirection = 1;
      //}
      //if(!input_line->output_redirection)
      //{
      //sprintf(input_line->outputFile,"%s","/dev/null");
      //input_line->output_redirection = 1;
      //}
    }
    else
    {
      signal(SIGINT, sigintChild);
    }

    if (input_line->input_redirection) //if >
    {
      inputFile_disc = open(input_line->inputFile, O_RDONLY, 0);

      if (inputFile_disc < 0)
      {
        printf("cannot open %s for input\n", input_line->argc[1]);
        exit(1);
      }
      dup2(inputFile_disc, STDIN_FILENO);
      close(inputFile_disc);
    }

    if (input_line->output_redirection) // if there is redirection
    {
      outputFile_disc = creat(input_line->outputFile, 0644);
      if (outputFile_disc < 0)
      {
        printf("cannot open %s for output\n", input_line->argc[1]);
        exit(1);
      }
      dup2(outputFile_disc, STDOUT_FILENO);
      close(outputFile_disc);
    }

    argc_list(input_line, argc);       // get the array for execp
    execvp(input_line->command, argc); //call execvp
    printf("%s: no such file or dirctory\n", input_line->command);
    fflush(stdout);
    exit(1);
  }
  else if (spawnpid < 0)
  {
    // Error forking
    perror("Error child");
  }
  else
  {
    //parent process
    if (input_line->background)
    {
      for (int i = 0; i < MAX_BG_P; i++)
      {
        if (open_processes[i] == 0)
        {
          open_processes[i] = spawnpid; // populating open_process to keep track of pids
          break;
        }
      }
      printf("background pid is: %d\n", spawnpid);
      fflush(stdout);
    }
    else
    {

      do
      {
        wait_pid = waitpid(spawnpid, &status, 0); // waits for foreground and print termination sig
        if (wait_pid > 0)
        {
          if (WIFSIGNALED(status)) //checking for child  termination
          {
            int exitStatus = WTERMSIG(status);
            printf("terminated by signal %d\n", exitStatus);
            fflush(stdout);
            return (exitStatus);
          }
          else if (WIFEXITED(status)) // if child exits normally
          {
            return (WEXITSTATUS(status));
          }
          else if (WIFSTOPPED(status))
          {
            int exitStatus = WSTOPSIG(status); // extra STOP signal to cover tracks just in case
            printf("terminated by signal %d\n", exitStatus);
            fflush(stdout);
            return exitStatus;
          }
        }
      } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status)); //looping through for status
    }
  }
  return status;
}

/*
where we determine built-in commands and execute them 
also where we call new_process whichs sets up our fork
*/
int execute_command(struct input *input_line, int *status, pid_t *open_processes)
{
  size_t size = sizeof input_line->argc / sizeof(char);
  pid_t cpid;

  //making sure command is printed
  //printf("%s\n",input_line->command);
  // making sure each argument after command can be accessed
  /*for(int i = 0;i<size;i++)
  { 
    if (input_line->argc[i] != NULL)
      printf("%s\n",input_line->argc[i]);
  }
  */

  // if both command and argc are empty, do nothing
  if (input_line->command == NULL)
  {
    //
  }

  else if (strcmp(input_line->command, "exit") == 0)
  {
    //exit and kill any other processes
    for (int i = 0; i < MAX_BG_P; i++)
    {
      if (open_processes[i] != 0)
      {
        kill(open_processes[i], SIGINT);
      }
    }
    //waiting to make sure parent and signal do not mix together
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 500;
    nanosleep(&ts, NULL);
    //where we kill all open processes and prints out messages for them before exiting
    for (int i = 0; i < MAX_BG_P; i++)
    {
      if (open_processes[i] != 0)
      {
        processStatus(open_processes[i]);
      }
    }
    return 0;
  }
  else if (strcmp(input_line->command, "cd") == 0) // if cd is entered
  {
    char *home = getenv("HOME");     //set home to home path
    if (input_line->argc[0] == NULL) //if only cd command present
    {
      chdir(home); // cd to home path
    }
    else
    {
      if (chdir(input_line->argc[0]) == -1) // if no path directory exists
        perror("chdir");
    }

    //change directory
    chdir(input_line->argc[0]); // changing dir if there is argument

    //testing cd changes
    //char cwd[100];
    //getcwd(cwd,sizeof(cwd));
    //printf("%s\n",cwd);
  }

  else if (strcmp(input_line->command, "status") == 0)
  {
    if (*status > 2) //done so we do not call it when its status &
    {
      printf("terminated by signal %d\n", *status);
    }
    else
    {
      printf("exit value %d\n", *status);
    }
  }

  // handle input and output file
  else
  {
    if (input_line->background)
    {
      newProcess(input_line, open_processes);
    }
    else
    {
      *status = newProcess(input_line, open_processes);
    }
  }

  return 1;
}