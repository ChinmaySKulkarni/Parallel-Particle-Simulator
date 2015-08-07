#include<stdio.h>
#include"distribution.decl.h"
#include<stdlib.h>
#include<errno.h>
#include<time.h>
#include <string.h>
#include<limits>
#define TOTAL_PARTICLES 10000
#define TOTAL_STEPS 1000

CProxy_main mainProxy;

class main: public CBase_main
{
	private:
	typedef struct node
	{
		double valX,valY;
		struct node *next;
	}node;

	typedef struct linkedList
	{
		node *head;
	}linkedList;

	linkedList list[8][8];					
	CProxy_points pointsProxy;
	int stage;						//To check for 100 iterations.
	int chkinCounter;					//To check that all chares are ready.
	int totalParticles;					//Used at startStep to check that number of particles is the same.
	double valuesListX[TOTAL_PARTICLES],valuesListY[TOTAL_PARTICLES];
								//To store x and y coordinates of (TOTAL_PARTICLES) number of 	points.

	void appendList(int listIndexX, int listIndexY,double Xcoordinate,double Ycoordinate)//Only occurs at stage 0
	{
		node *tmp,*p;
		tmp = (node *)malloc(sizeof(node));
		
		if(tmp == NULL)
		{
			perror("Appending error! \n");
			CkExit();
		}
		tmp->valX = Xcoordinate;
		tmp->valY = Ycoordinate;
		tmp->next = NULL;
		
		if(list[listIndexX][listIndexY].head == NULL)
		{
			list[listIndexX][listIndexY].head = tmp;		
			return;
		}
		p = list[listIndexX][listIndexY].head;
		while(p->next)
			p = p->next;
		p->next = tmp;
	};

	void copyVal(linkedList l,int indexX,int indexY)				//Only occurs at stage 0
	{
		int i = 0;
		node *p;
		p = l.head;
		while(p)
		{
			valuesListX[i] = p->valX;
			valuesListY[i] = p->valY;
			i++;
			p = p->next;
		}
		pointsProxy[CkArrayIndex2D(indexX,indexY)].assignPoints(valuesListX, valuesListY,i);
	};

	void startStep()
	{	
		chkinCounter = 0;
		if(stage == 0)								//Initial distribution of points amongst chares
		{
			CkPrintf("The points are being distributed amongst chares. \n");
			int tempX,tempY,i,j;
			double Xcoordinate,Ycoordinate;
			stage++;
			for(i=0;i<=7;i++)
			{
				for(j=0;j<=7;j++)
					list[i][j].head = NULL;
			}
			for(i=0;i<TOTAL_PARTICLES;i++)
			{
				Xcoordinate = (double)rand()/(double)RAND_MAX;	
				Ycoordinate = (double)rand()/(double)RAND_MAX;	
				tempX = (Xcoordinate*8);
				tempY = (Ycoordinate*8);
				appendList(tempX,tempY,Xcoordinate, Ycoordinate);
			}	
			CkPrintf("There are %d particles \n", TOTAL_PARTICLES);
			for(i=0;i<=7;i++)
			{
				for(j=0;j<=7;j++)
					copyVal(list[i][j],i,j);
			}
		}

		else if((stage>0)&&(stage <TOTAL_STEPS))
		{
			totalParticles = 0;		
			stage++;					/*Re-initialized because we want to check for each 											  iteration*/				
			pointsProxy.shiftPoints();			//Broadcast to all chares.		
		}
		
		else if(stage == TOTAL_STEPS)
		{
			if(totalParticles == TOTAL_PARTICLES)				
			{
				CkPrintf("There are still %d particles.\n", totalParticles);
				CkPrintf("Program finished successfully! \n");
			}
			else
                        	CkPrintf("%d particles lost! \n",TOTAL_PARTICLES - totalParticles);
			CkExit();
		}
	};

	public:
	main(CkMigrateMessage *msg)
	{};
	
	main(CkArgMsg *m)
	{
		delete m;
		CkPrintf("%d Random points(x,y) between 0 and 1 being generated. On Total PEs: [%d] \n", TOTAL_PARTICLES, CkNumPes());
		mainProxy = thisProxy;
		pointsProxy = CProxy_points::ckNew(8,8);
		srand((unsigned)time(NULL));
		stage = 0;
		totalParticles = 0;
		startStep();			

	};

	void checkIn(int recdPoints)
	{
		chkinCounter++;
		totalParticles = totalParticles + recdPoints; 	
		if(chkinCounter == 64)
		{
			if(stage == 0)
				stage = 1;
			CkPrintf("Stage %d Total CheckIns : %d with %d particles. \n",stage,chkinCounter,totalParticles);
			startStep();
		}
	};
};

/******************************************************2D CHARE ARRAY CLASS*********************************************************************/ 

class points: public CBase_points
{
	private:
	double *valuesX;
	double *valuesY;
	int count;
	int ghostCount;
	int totalPoints;
	double ghostX[10000];
	double ghostY[10000];
		
/*********************************************************************************************************************************************
NOTE:		http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/   

realloc() will keep the size of the specified pointer to the value given by the user. The data remains unchanged. If the new value assigned to the pointer is larger than the previous memory size of that pointer, the newly allocated memory will be indeterminate (NOT NULL)! If the new value is less, then the correspoding data upto that value stays and the rest is freed...[Keep in mind to avoid seg faults]

******In case that ptr is NULL, the function behaves exactly as malloc, assigning a new block of size bytes and returning a pointer to the beginning of it.******

In case that the size is 0, the memory previously allocated in ptr is deallocated as if a call to free was made, AND a NULL pointer is returned(unlike just "free").
**********************************************************************************************************************************************/


	void startStep()
	{
		int i,k,lostIndex;
		double *tmpBufX = (double *)malloc(sizeof(double)*count);
		double *tmpBufY = (double *)malloc(sizeof(double)*count);
		for(i = 0;i<count;i++)
		{
			tmpBufX[i] = valuesX[i];			
			tmpBufY[i] = valuesY[i];
		}		
		free(valuesX);
		free(valuesY);
		double *outBufX, *outBufY,*blankGhostX,*blankGhostY;
		blankGhostX = NULL;						//If no points go to a neighbor, we send a blank ghost message.
		blankGhostY = NULL;
		outBufX = NULL;							//temporary buffers used to store travelling particles.
		outBufY = NULL; 
		int j = 0;
		for(i = 0;i<count;i++)					//Case 1: Send NW
		{
			if(((tmpBufX[i]*8.0) < thisIndex.x) && ((tmpBufY[i]*8.0) < thisIndex.y))
			{
				outBufX = (double *)realloc(outBufX,sizeof(double)*(j+1));					
				outBufY = (double *)realloc(outBufY,sizeof(double)*(j+1));					
				outBufX[j] = tmpBufX[i];
				outBufY[j] = tmpBufY[i];
				lostIndex = i;
				for(k=i+1;k<count;k++,i++)		//delete the particle going out of the chare's boundaries.
				{
					tmpBufX[i] = tmpBufX[k];
					tmpBufY[i] = tmpBufY[k];
				}
				i = lostIndex;
				count--;				//decrement count. 
				j++;
			}	
		}
		if(j!=0)			
			thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, (thisIndex.y -1 + 8)%8)].Ghost(outBufX, outBufY,j);
		else
			thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, (thisIndex.y -1 + 8)%8)].Ghost(blankGhostX, blankGhostY,0);



		j = 0;
		for(i = 0;i<count;i++)					//Case 2: Send WW
		{
			if(((tmpBufX[i]*8.0) < thisIndex.x) && ((tmpBufY[i]*8.0) >= thisIndex.y) && ((tmpBufY[i]*8.0)<= (thisIndex.y + 1)))
			{
				outBufX = (double *)realloc(outBufX,sizeof(double)*(j+1));					
				outBufY = (double *)realloc(outBufY,sizeof(double)*(j+1));						
				outBufX[j] = tmpBufX[i];		//rewrite to temporary buffer
				outBufY[j] = tmpBufY[i];
				lostIndex = i;
				for(k=i+1;k<count;k++,i++)		
				{
					tmpBufX[i] = tmpBufX[k];
					tmpBufY[i] = tmpBufY[k];
				}
				i = lostIndex;
				count--;				
				j++;
			}	
		}
		if(j!=0)
			thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, thisIndex.y)].Ghost(outBufX, outBufY,j);
		else 
			thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, thisIndex.y)].Ghost(blankGhostX, blankGhostY,0);



		j = 0;
		for(i = 0;i<count;i++)					//Case 3: Send SW
		{
			if(((tmpBufX[i]*8.0) < thisIndex.x) && ((tmpBufY[i]*8.0) > (thisIndex.y + 1)))
			{
				outBufX = (double *)realloc(outBufX,sizeof(double)*(j+1));					
				outBufY = (double *)realloc(outBufY,sizeof(double)*(j+1));						
				outBufX[j] = tmpBufX[i];
				outBufY[j] = tmpBufY[i];
				lostIndex = i;
				for(k=i+1;k<count;k++,i++)		
				{
					tmpBufX[i] = tmpBufX[k];
					tmpBufY[i] = tmpBufY[k];
				}
				i = lostIndex;
				count--;				
				j++;
			}	
		
		}
		if(j!=0)
			thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, (thisIndex.y + 1)%8)].Ghost(outBufX, outBufY,j);
		else
			thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, (thisIndex.y + 1)%8)].Ghost(blankGhostX, blankGhostY,0);
	


		j = 0;
		for(i = 0;i<count;i++)					//Case 4: Send SS
		{
			if(((tmpBufX[i]*8.0)>=thisIndex.x) && ((tmpBufX[i]*8.0)<=(thisIndex.x + 1)) && ((tmpBufY[i]*8.0)>(thisIndex.y+ 1)))
			{
				outBufX = (double *)realloc(outBufX,sizeof(double)*(j+1));					
				outBufY = (double *)realloc(outBufY,sizeof(double)*(j+1));						
				outBufX[j] = tmpBufX[i];
				outBufY[j] = tmpBufY[i];
				lostIndex = i;
				for(k=i+1;k<count;k++,i++)		
				{
					tmpBufX[i] = tmpBufX[k];
					tmpBufY[i] = tmpBufY[k];
				}
				i = lostIndex;
				count--;				 
				j++;
			}	
		}
		if(j!=0)
			thisProxy[CkArrayIndex2D(thisIndex.x, (thisIndex.y + 1)%8)].Ghost(outBufX, outBufY,j);
		else
			thisProxy[CkArrayIndex2D(thisIndex.x, (thisIndex.y + 1)%8)].Ghost(blankGhostX, blankGhostY,0);



		j = 0;
		for(i = 0;i<count;i++)					//Case 5: Send SE
		{
			if(((tmpBufX[i]*8.0) > (thisIndex.x + 1)) && ((tmpBufY[i]*8.0) > (thisIndex.y + 1)))
			{
				outBufX = (double *)realloc(outBufX,sizeof(double)*(j+1));					
				outBufY = (double *)realloc(outBufY,sizeof(double)*(j+1));						
				outBufX[j] = tmpBufX[i];
				outBufY[j] = tmpBufY[i];
				lostIndex = i;
				for(k=i+1;k<count;k++,i++)		
				{
					tmpBufX[i] = tmpBufX[k];
					tmpBufY[i] = tmpBufY[k];
				}
				i = lostIndex;
				count--;				 
				j++;
			}	
		}
		if(j!=0)
			thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, (thisIndex.y + 1)%8)].Ghost(outBufX, outBufY,j);
		else
			thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, (thisIndex.y + 1)%8)].Ghost(blankGhostX, blankGhostY,0);


		
		j = 0;
		for(i = 0;i<count;i++)					//Case 6: Send EE
		{
			if(((tmpBufX[i]*8.0)>(thisIndex.x + 1))&&((tmpBufY[i]*8.0) <= (thisIndex.y + 1))&&((tmpBufY[i]*8.0)>= thisIndex.y))
			{
				outBufX = (double *)realloc(outBufX,sizeof(double)*(j+1));					
				outBufY = (double *)realloc(outBufY,sizeof(double)*(j+1));						
				outBufX[j] = tmpBufX[i];
				outBufY[j] = tmpBufY[i];
				lostIndex = i;
				for(k=i+1;k<count;k++,i++)		
				{
					tmpBufX[i] = tmpBufX[k];
					tmpBufY[i] = tmpBufY[k];
				}
				i = lostIndex;
				count--;				 
				j++;
			}	
		}
		if(j!=0)
			thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, thisIndex.y)].Ghost(outBufX, outBufY,j);
		else
			thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, thisIndex.y)].Ghost(blankGhostX, blankGhostY,0);


		
		j = 0;
		for(i = 0;i<count;i++)					//Case 7: Send NE
		{
			if(((tmpBufX[i]*8.0) > (thisIndex.x + 1)) && ((tmpBufY[i]*8.0) < thisIndex.y))
			{
				outBufX = (double *)realloc(outBufX,sizeof(double)*(j+1));					
				outBufY = (double *)realloc(outBufY,sizeof(double)*(j+1));						
				outBufX[j] = tmpBufX[i];
				outBufY[j] = tmpBufY[i];
				lostIndex = i;
				for(k=i+1;k<count;k++,i++)		
				{
					tmpBufX[i] = tmpBufX[k];
					tmpBufY[i] = tmpBufY[k];
				}
				i = lostIndex;
				count--;				 
				j++;
			}	 
		}
		if(j!=0)
			thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, (thisIndex.y -1 + 8)%8)].Ghost(outBufX, outBufY,j);
		else 
			thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, (thisIndex.y -1 + 8)%8)].Ghost(blankGhostX, blankGhostY,0);



		j = 0;
		for(i = 0;i<count;i++)					//Case 8: Send NN
		{
			if(((tmpBufX[i]*8.0) >= thisIndex.x) && ((tmpBufX[i]*8.0)<= (thisIndex.x + 1)) && ((tmpBufY[i]*8.0) < thisIndex.y))
			{
				outBufX = (double *)realloc(outBufX,sizeof(double)*(j+1));					
				outBufY = (double *)realloc(outBufY,sizeof(double)*(j+1));						
				outBufX[j] = tmpBufX[i];
				outBufY[j] = tmpBufY[i];
				lostIndex = i;
				for(k=i+1;k<count;k++,i++)		
				{
					tmpBufX[i] = tmpBufX[k];
					tmpBufY[i] = tmpBufY[k];
				}
				i = lostIndex;
				count--;				 
				j++;
			}	
		}
		if(j!=0)
			thisProxy[CkArrayIndex2D(thisIndex.x, (thisIndex.y -1 + 8)%8)].Ghost(outBufX, outBufY,j);
		else
			thisProxy[CkArrayIndex2D(thisIndex.x, (thisIndex.y -1 + 8)%8)].Ghost(blankGhostX, blankGhostY,0);

		valuesX = (double *)malloc(sizeof(double)*count);
		valuesY = (double *)malloc(sizeof(double)*count);
		for(i = 0;i<count;i++)
		{
			valuesX[i] = tmpBufX[i];			
			valuesY[i] = tmpBufY[i];
		}	
		free(outBufX);						//Free the memory assigned to buffers.
		free(outBufY);
		free(tmpBufX);
		free(tmpBufY);
		free(blankGhostX);
		free(blankGhostY);
	};
										/****free only deallocates the memory assigned. It does 										     not assign a NULL pointer****IMPORTANT to 											     avoid Seg Faults!
										http://www.cplusplus.com/reference/clibrary/cstdlib/free/
										****/
	public:		
	points(CkMigrateMessage *msg)
	{};
	
	points()
	{
		count = 0;						//Number of points stored by each chare.(initially 0).
		ghostCount = 0;						//How many ghost messages have been received by the chare.
		valuesX = NULL;
		valuesY = NULL;
		totalPoints = 0;					//Total number of points that have shifted into that chare.
	};

	void assignPoints(double *valuesListX,double *valuesListY,int total)
	{								//Used only in stage 0 to assign points to the chares.
		count = total;						//total count initialized here.
		int i;
		valuesX = (double *)malloc(sizeof(double)*count);
		valuesY = (double *)malloc(sizeof(double)*count);
		for(i = 0;i<count;i++)
		{
			valuesX[i] = valuesListX[i];
			valuesY[i] = valuesListY[i];
		}
		mainProxy.checkIn(count);
	};

	void shiftPoints()
	{
		int i;							//Generate random doubles between (-1/800 to 1/800)
		double shiftX = (((double)rand()/(double)RAND_MAX)/400 - (1.0/800.0));
		double shiftY = (((double)rand()/(double)RAND_MAX)/400 - (1.0/800.0));
		double tempX,tempY;
		for(i = 0;i<count;i++)
		{	
			tempX = valuesX[i];
			tempY = valuesY[i];
			valuesX[i] = valuesX[i]+shiftX;
			valuesY[i] = valuesY[i]+shiftY;
			if(valuesX[i] < 0.0)				//Adjustments done to ensure that the coordinates lie within [0,1].
				valuesX[i] = 0.0 - valuesX[i];
			else if(valuesX[i] > 1.0)
				valuesX[i] = tempX;
			if(valuesY[i] < 0.0)
				valuesY[i] = 0.0 - valuesY[i];
			else if(valuesY[i] > 1.0)
				valuesY[i] = tempY;
		}
		startStep();
	};

	void Ghost(double *outBufX, double *outBufY, int pointsReceived)	//Receive and process ghosts.
	{
		ghostCount++;
		int p;			
		for(p=totalPoints;p<(pointsReceived + totalPoints);p++)
		{
			ghostX[p] = outBufX[p - totalPoints];
			ghostY[p] = outBufY[p - totalPoints];
		}
		totalPoints = totalPoints + pointsReceived;
		if(ghostCount == 8)
		{				
			valuesX = (double *)realloc(valuesX,sizeof(double)*(count + totalPoints));//NOTE (count+totalPoints)!! 
			valuesY = (double *)realloc(valuesY,sizeof(double)*(count + totalPoints));
			int i;
			for(i=count;i<(totalPoints + count);i++)
			{
				valuesX[i] = ghostX[i - count];
				valuesY[i] = ghostY[i - count];
			}
			count = count + totalPoints;				//Now each chare contains these many points.
			ghostCount = 0;						//Re-initialize these variables to zero.
			totalPoints = 0;
			mainProxy.checkIn(count);
		}
	};
};
			
#include"distribution.def.h"

			
