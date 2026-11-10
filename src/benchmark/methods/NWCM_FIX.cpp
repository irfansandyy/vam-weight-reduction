#include<stdio.h>
#include<conio.h>
#include<stdlib.h>
int cost,i,j,msizerow,msizecol,matrix[100][100];
int r=0,c=0,p;
int supply[100],demand[100];
int value[100][100],ppos,cpos,rpre,cpre;
void firstiteration();

int main()
{
		
	FILE *in;
	in=fopen("p1717.txt","r");
	
	while(!feof(in))
	{
		fscanf(in,"%d", &msizerow);	
		fscanf(in,"%d", &msizecol);

			for(r=0;r<msizerow;r++)
			{	
				// Input Cost
				for(c=0;c<msizecol;c++)
				{
					fscanf(in,"%d",&matrix[r][c]);
					//temp_cost[i][j] = cost[i][j];
					//printf("cost = %d\n",cost[i][j]);
					//printf("temp cost = %d\n",temp_cost[i][j]);
				}
			}
			
			// Input Supply
			for(r=0;r<msizerow;r++)
			{
				fscanf(in,"%d",&supply[r]);
				//total_supply+=supply[i];
				//supply_awal[i]=supply[i];
			}
			
			// Input Demand
			for(c=0;c<msizecol;c++)
			{
				fscanf(in,"%d",&demand[c]);
				//total_demand+=demand[i];
			}
	
	c=0;r=0;cost=0;
    do{
        if(demand[c]!=0)
        {
            if(demand[c]<supply[r])
            {
                value[r][c]=demand[c];
                supply[r]-=demand[c];
                demand[c]=0;
                if(c==msizecol-1)
                    c=0;
                else
                    c++;
            }
            else
            {
                if(demand[c]==supply[r])
                {
                    value[r][c]=supply[r];
                    demand[c]=0;
                    supply[r]=0;
                    if(c==msizecol-1)
                        c=0;
                    else
                        c++;
                    if(r==msizerow-1)
                        r=0;
                    else
                        r++;
                }
                else
                {
                    value[r][c]=supply[r];
                    demand[c]-=supply[r];
                    if(r==msizerow-1)
                        r=0;
                    else
                        r++;
                }
            }
        }
        else
        {
            if(c==msizecol-1)
                c=0;
            else
                c++;
        }
    }
        while(demand[msizecol-1]!=0);
        printf("\n");
        for(r=0;r<msizerow;r++)
        {
            for(c=0;c<msizecol;c++)
            {
                printf("\n Value[%d][%d]=%d",r+1,c+1,value[r][c]);
            }
        }


    printf("\n The Transportation Problem \n");
      for(r=0;r<msizerow;r++)
         {
            for(c=0;c<msizecol;c++)
            {
                printf("\t%d",matrix[r][c]);
                //cost+=matrix[r][c]*value[r][c];
            }
         printf("\n");
        }

    printf("\n\n The Initial Matrix \n");
      for(r=0;r<msizerow;r++)
         {
            for(c=0;c<msizecol;c++)
            {
                printf("\t%d",value[r][c]);
                //cost+=matrix[r][c]*value[r][c];
            }
         printf("\n");
        }
        for(r=0;r<msizerow;r++)
         {
            for(c=0;c<msizecol;c++)
            {
                cost+=matrix[r][c]*value[r][c];
            }
        }
      printf("\n Cost=%d",cost);
	
//  getch();
 }
}

