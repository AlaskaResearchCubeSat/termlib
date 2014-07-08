#ifndef __TERMINAL_H
#define __TERMINAL_H
  
  //structure for describing commands
  typedef struct{
    const char* name;
    const char* helpStr;
    //function pointer to command
    int (*cmd)(char **argv,unsigned short argc);
  }CMD_SPEC;
  
  //table of commands with help
  extern const CMD_SPEC cmd_tbl[];
  
  //Command to list commands or give help on a specific command
  int helpCmd(char **argv,unsigned short argc);
  //task to run terminal commands
  void terminal(void *p);

  //split string into arguments
  unsigned short make_args(char *argv[],const char *src,char *dst);


#endif
  