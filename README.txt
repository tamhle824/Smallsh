Simple shell in C that does the following:

Provides a prompt for running commands
Handles blank lines and comments
Provides expansion for the variable $$
Executes 3 commands exit, cd, and status
Executes other commands by creating new processes using a function from the exec family of functions
Supports input and output redirection
Supports running commands in foreground and background processes
Implements custom handlers for 2 signals, SIGINT and SIGTSTP

type in gcc --std=gnu99 -o smallsh main.c

to run code, type ./smallsh



To run the script, place it in the same directory as your compiled 
shell, chmod it (chmod +x ./p3testscript) and run this command from a bash prompt:


type in ./p3testscript 2>&1

or ./p3testscript > mytestresults 2>&1 