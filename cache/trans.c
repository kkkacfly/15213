/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{

    int ii, jj, i, j;
    int block, blockR, blockC;
    int v0,v1,v2,v3,v4,v5,v6,v7;
    int temp;
    int* t;

    REQUIRES(M > 0);
    REQUIRES(N > 0);


    if(M == 32)
    {         
    	block = 8;
    	for(ii = 0; ii < N/block; ii++)
    	{
    	    for(jj = 0; jj < M/block; jj++)
            {
                for(i = 0; i < block; i++)
        	{
        	    for(j = 0; j < block; j++)
                    {
        		 if(ii==jj && i==j)
        		 {
        		     temp = A[i+ii*block][j+jj*block];
        	         }
        	         else
        	         {
        		     B[j+jj*block][i+ii*block] = A[i+ii*block][j+jj*block];
        	         }
        	     }
        	     /* load diagonal element */
        	     if(ii == jj)
        	     {
        	          B[i+ii*block][i+ii*block] = temp;
        	     }
                }
    	    }
    	}
    }


    if (N == 64) 
    {
	for(blockR = 0; blockR < N; blockR += 8)
	{
            for(blockC = 0; blockC < M; blockC += 8)
	    {
                for(i = 0; i < 8; i++)
		{
                    t = &A[blockC + i][blockR];
                    v0 = t[0];
                    v1 = t[1];
                    v2 = t[2];
                    v3 = t[3];

                    if(!i)
		    {
                        v4 = t[4];
                        v5 = t[5];
                        v6 = t[6];
                        v7 = t[7];
                    }

                    t = &B[blockR][blockC + i];
                    t[0] = v0;
                    t[64] = v1;
                    t[128] = v2;
                    t[192] = v3;
                }
		 for(i = 7; i > 0; i--)
		 {
                    t = &A[blockC+i][blockR+4];
                    v0 = t[0];
                    v1 = t[1];
                    v2 = t[2];
                    v3 = t[3];
                    t = &B[blockR+4][blockC+i];
                    t[0] = v0;
                    t[64] = v1;
                    t[128] = v2;
                    t[192] = v3;
                }

                t = &B[blockR + 4][blockC];
                t[0] = v4;
                t[64] = v5;
                t[128] = v6;
                t[192] = v7;
            }
        }
             							
    } 

    if (M == 61)
    {
        block = 18;		
        for (blockR = 0; blockR < N; blockR += block)
	{
            for (blockC = 0; blockC < M; blockC += block)
	    {
                for (i = blockR; i < blockR + block && i < N; ++i)
		{
                    for (j = blockC; j < blockC + block && j < M; ++j)
		    {
                        temp = A[i][j];
                        B[j][i] = temp;
		    }
		}
	    }
	}
    }
	
    ENSURES(is_transpose(M, N, A, B));
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++){ 
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
    
     ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

