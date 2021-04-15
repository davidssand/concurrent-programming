#include <stdio.h>
#include <ctype.h>
#include <conio.h>

int main(void)
{
    char ch;    // to store an input - single char
    int number = 0; // to make number from inputs
    printf("Enter a number: ");
    int digits_cnt = 0;
    while (digits_cnt < 4)
    {
        ch = _getche();
        if (isdigit(ch))
        {
            number *= 10;  // add an order to number
            number += ch - '0';  // add a decimal digit to number
            digits_cnt++;  // count this digit to stop loop
        }
    }
    // just to check result
    printf("\nThe number %d was entered.\n", number);
    return 0;
}