#include <stdio.h>
#include <stdlib.h>

int main()
{
   int num = 124235;
   FILE *fptr;

   // use appropriate location if you are using MacOS or Linux
   fptr = fopen("test.csv","w");

   if(fptr == NULL)
   {
      printf("Error!");   
      exit(1);             
   }

   fprintf(fptr,"%d\n",num);
   fprintf(fptr,"%d\n",num);
   fclose(fptr);

   return 0;
}