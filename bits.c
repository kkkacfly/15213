/* 
 * CS:APP Data Lab 
 * 
 * <Please put your name and userid here>
 * Name: Xuan Li;
 * UserID: xuanli1; 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
/* 
 * bitAnd - x&y using only ~ and | 
 *   Example: bitAnd(6, 5) = 4
 *   Legal ops: ~ |
 *   Max ops: 8
 *   Rating: 1
 */
int bitAnd(int x, int y) {
 
    return ~(~x | ~y);
}

/* 
 * tmin - return minimum two's complement integer 
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmin(void) {
    return (1 << 31);
}
/* 
 * negate - return -x 
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x) {

  return (~x + 1);
}
/* 
 * allEvenBits - return 1 if all even-numbered bits in word set to 1
 *   Examples allEvenBits(0xFFFFFFFE) = 0, allEvenBits(0x55555555) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 2
 */
int allEvenBits(int x) {
    int y;
    int yy = 0xAA << 8 | 0xAA;
    yy = yy << 16 | yy;
    /* now yy is the bit has '1' at odd position */
    y = yy | x;
    /* y judges whether x has all '1' at even pos (return all 1) */

    return !(~y);
}
/*
 * bitCount - returns count of number of 1's in word
 *   Examples: bitCount(5) = 2, bitCount(7) = 3
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 40
 *   Rating 4
 */
int bitCount(int x) {
/*
 *  divide x into 8 groups with 4 bits each
 *  sum the '1's in each group and then sum them together
 */   
    int mask = 0x11;
    int mask2 = 0xf;
    int sum1;
    int sum2;
    mask = (mask << 8) | mask;
    mask = (mask << 16) | mask;
    /* now the mask is of the form 00010001....0001 */
    sum1 = mask & x;
    sum1 = sum1 + (mask & (x >> 1));
    sum1 = sum1 + (mask & (x >> 2));
    sum1 = sum1 + (mask & (x >> 3));
    sum1 = sum1 + (sum1 >> 16);
    /* now the sum of 1 in each group is saved in sum1 */
    sum2 = sum1 & mask2;
    sum2 = sum2 + ((sum1 >> 4) & mask2);
    sum2 = sum2 + ((sum1 >> 8) & mask2);
    sum2 = sum2 + ((sum1 >> 12) & mask2);

    return sum2;
}
/* 
 * logicalShift - shift x to the right by n, using a logical shift
 *   Can assume that 0 <= n <= 31
 *   Examples: logicalShift(0x87654321,4) = 0x08765432
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 3 
 */
int logicalShift(int x, int n) {
/* 
 * create mask1 like 00011..1111 and mask2 like 0010...0000
 * the first would keep the later part of the shifted value
 * while the  second one keep the first No. of the shifted value
 */ 
    int con1 = ~0;
    int con2 = 31 + ~n + 1;
    int var1 = x >> n;
    int mask1 = con1 ^ (con1 << con2);
    int mask2 = 1 << con2;

    return (mask1 & var1) | (mask2 & var1);
}
/* 
 * isNegative - return 1 if x < 0, return 0 otherwise 
 *   Example: isNegative(-1) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
int isNegative(int x) {

    return !!(x >> 31);
}
/* 
 * isGreater - if x > y  then return 1, else return 0 
 *   Example: isGreater(4,5) = 0, isGreater(5,4) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isGreater(int x, int y) {
/* 
 *  two cases: x>=0 y<0; or x-y>0 when they are of the same sign (not equal)
 */
    int judge1 = ~(x ^ y) & (y + ~x + 1);
    int judge2 = y & (x ^ y);

    return !!((judge1 | judge2) >> 31);
 
}
/*
 * isPower2 - returns 1 if x is a power of 2, and 0 otherwise
 *   Examples: isPower2(5) = 0, isPower2(8) = 1, isPower2(0) = 0
 *   Note that no negative number is a power of 2.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 4
 */
int isPower2(int x) {
/* 
 * x & (x-1) would be zero if x is power of two;
 * also x must not be 0 or negative;
 */
    int judge1 = !(x & (~0 + x));
    int judge2 = (!(x >> 31)) & (!!(x ^ 0));
    return judge1 & judge2;    
}
/* 
 * fitsBits - return 1 if x can be represented as an 
 *  n-bit, two's complement integer.
 *   1 <= n <= 32
 *   Examples: fitsBits(5,3) = 0, fitsBits(-4,3) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 2
 */
int fitsBits(int x, int n) {
/* 
 * first rightshift x (n-1) positions, if x could be expressed with n digits,
 * for postive int it would return 0000....0000
 * for negative int it would return 1111....1111
 * then judge whether the result is of this form
 */
    x = (x >> (n + ~1 + 1));
    return (!(x) ^ !(~x));
}
/* 
 * conditional - same as x ? y : z 
 *   Example: conditional(2,4,5) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3
 */
int conditional(int x, int y, int z) {
/*
 * if x is 0, we create the mask 000...000
 * if x is not zero, we create the mask 111...111 
 * use the first mask with y and use the second with z and use | to get result
 */
    x = !!x;
    x = ~x + 1;

    return (x & y) | (~x & z);
}
/* 
 * greatestBitPos - return a mask that marks the position of the
 *               most significant 1 bit. If x == 0, return 0
 *   Example: greatestBitPos(96) = 0x40
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 70
 *   Rating: 4 
 */
int greatestBitPos(int x) {
/* 
 * shift x towards right and make x the form 00111...111 if x is positive;
 * get the number;
 * also consider x begins with 1 (negative);
 */
    int a = x | (x >> 1);
    a = a | (a >> 2);
    a = a | (a >> 4);
    a = a | (a >> 8);
    a = a | (a >> 16);
    return ((a ^ (a >> 1)) | 1 << 31) & a;
}
/* 
 * float_i2f - Return bit-level equivalent of expression (float) x
 *   Result is returned as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point values.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_i2f(int x) {
/* 
 * this solution considers four parts: the sign digit (sign); the eight expo number;
 * the major part xx; and rounding number (adjust)
 */

    unsigned sign, p, adjust, tempt, xx;

    if(x == 0)
    return 0;

    sign=0;

    xx = x;
    if(x < 0)
    {sign = (1 << 31);
     xx = ~x + 1;}

/* p determines the No. of expo */
    p = 0;
    adjust = 0;

/* 
 * in this process we get the most significant number of x and count how many bits we move,
 * the bits we move is the result and we assign it according to the float rules
 */
    while(1)
    {
     tempt = xx;
     xx = xx << 1;
     p++;
     if(tempt & 0x80000000)
     break;
    }

/* if the fraction is greater than 1/2, round it to 1; if it is 1/2 and there is 1 in the major part, round it to 1 also */ 
    if((xx & 0x01ff) > 0x0100) adjust = 1;
    if((xx & 0x03ff) == 0x0300) adjust = 1;

    return (sign + (xx >> 9) + ((127 + 32 - p ) << 23) + adjust);
}
/* 
 * float_abs - Return bit-level equivalent of absolute value of f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representations of
 *   single-precision floating point values.
 *   When argument is NaN, return argument..
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 10
 *   Rating: 2
 */
unsigned float_abs(unsigned uf) {
/*
 * first judge whether uf is NaN and then get result;
 */
    unsigned a = 0x7FFFFFFF;    
    unsigned min = 0x7F800001;  
    unsigned tempt = uf & a;  
    if (tempt >= min) 
        return uf;   
    else
        return tempt;  
}
