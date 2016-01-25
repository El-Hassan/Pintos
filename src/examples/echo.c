#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;

 //if(argv[][0] == 'h' && argv[1][1] == 'a' && argv[1][2] == 's' &&  argv[1][3] == '\0'){
//	return EXIT_SUCCESS;
//}

  for (i = 0; i < argc; i++)
    printf ("%s ", argv[i]);
  printf ("\n");

  return EXIT_SUCCESS;
}
