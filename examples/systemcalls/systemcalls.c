#include "systemcalls.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
/**
 * @param cmd the command to execute with system()
 * @return true if the commands in ... with arguments @param arguments were executed 
 *   successfully using the system() call, false if an error occurred, 
 *   either in invocation of the system() command, or if a non-zero return 
 *   value was returned by the command issued in @param.
*/
bool do_system(const char *cmd)
{

  if (system(cmd) != 0)
  {
    return false;  
  }

  return true;

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success 
 *   or false() if it returned a failure
*/
   
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the 
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/
//int execv(const char *path, char *const argv[]);
bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count+1];

    //memset(command, 0, count);
    
    printf("\n\n************************************************");
    printf("size of command : %lu , %d", sizeof(command), count);

    

    for(int i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
        printf("\ncommand[%d] :%s", i, command[i]);
    }
    
    command[count] = NULL;
    pid_t process_id;
    int status;

    process_id = fork();
    if(process_id == -1)
    {
        printf("\nfork failed");
        return false;
    }

    // int ret_exec = -1;
    if (process_id == 0) {
        printf("\nExecutiing execv : ");
        for(int i=0; i<count; i++)
        {
            printf("%s ", command[i]);
        } 
        printf("\n");
        execv(command[0], &command[1]); 
         exit(-1);
    }


    // if(ret_exec == -1)
    // {
    //     printf("\nexec failed");
    //     return false;
    // }
    printf("\nExec done--------------------after");


    
    if(waitpid(process_id, &status, 0) == -1)
    {
        printf("\nwait failed");
        return false;
    }
    else
    {
        printf("\nwait succeeded");
    }

    printf("\n============================%d=================================\n",status);
    if(status != 0)
        return false;


/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *   
*/

    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.  
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    command[count] = command[count];


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *   
*/

    va_end(args);
    
    return true;
}
