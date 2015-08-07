#include<stdio.h>
#include"load_balancing.decl.h"
#include<stdlib.h>
#include<errno.h>
#include<time.h>
#include<limits>
#define LB_INTERVAL 5 
#define TOTAL_PARTICLES 10000
#define TOTAL_STEPS 100

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
				int alternator = 0;
				srand((unsigned)time(NULL));
			
				for(i=0;i<TOTAL_PARTICLES;i++)
				{
					Xcoordinate = (double)rand()/(double)RAND_MAX;	
					Ycoordinate = (double)rand()/(double)RAND_MAX;	
					tempX = (Xcoordinate*8);
					tempY = (Ycoordinate*8);
					/*if((tempX <= 7) && (tempX >= 1) && (tempY <= 7) && (tempY >= 1))
					{
						if(alternator == 0)
						{
							tempX = 0;
							tempY = 0;
						}
						else 
						{
							tempX = 7;
							tempY = 7;
						}	
						alternator = ((alternator + 1)%2);
					}*/
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
				/*if(++stage % LB_INTERVAL == 0)
				{
					CkPrintf("Load balancing for stage: %d \n",stage);
					pointsProxy.balanceLoad();
				}
				else*/
					pointsProxy.shiftPoints();			//Broadcast to all chares.		
			}
			
			else if(stage == TOTAL_STEPS)
			{
				if(totalParticles == TOTAL_PARTICLES)				
				{
					CkPrintf("There are still %d particles.\n",totalParticles);
					CkPrintf("Program finished successfully! \n");
				}
				else
					CkPrintf("Some particles lost! \n");
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
			stage = 0;
			totalParticles = 0;
			startStep();			
	
		};
	
		void checkIn(int recdPoints)
		{
			totalParticles = totalParticles + recdPoints; 			
			chkinCounter++;
			if(chkinCounter == 64)
			{
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
		
/*********************************************************************************************************************************************
NOTE:		http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/   

realloc() will keep the size of the specified pointer to the value given by the user. The data remains unchanged. If the new value assigned to the pointer is larger than the previous memory size of that pointer, the newly allocated memory will be indeterminate (NOT NULL)! If the new value is less, then the correspoding data upto that value stays and the rest is freed...[Keep in mind to avoid seg faults]

******In case that ptr is NULL, the function behaves exactly as malloc, assigning a new block of size bytes and returning a pointer to the beginning of it.******

In case that the size is 0, the memory previously allocated in ptr is deallocated as if a call to free was made, AND a NULL pointer is returned(unlike just "free").
**********************************************************************************************************************************************/


		void startStep()
		{
			CkPrintf("Chare no. [%d][%d] .Total Points: %d \n",thisIndex.x, thisIndex.y, count);
			int i,k,lostIndex;
			double *tmpBufX, *tmpBufY,*blankGhostX,*blankGhostY;
			blankGhostX = (double *)malloc(sizeof(double));		//If no points go to a neighbor, we send a blank ghost message.
			blankGhostY = (double *)malloc(sizeof(double));
			memset(blankGhostX, 0, sizeof(double));
			memset(blankGhostY, 0, sizeof(double));
			tmpBufX = NULL;						//temporary buffers used to store travelling particles.
			tmpBufY = NULL; 
			int j = 0;
			for(i = 0;i<count;i++)					//Case 1: Send NW
			{
				if(((valuesX[i]*8.0) < thisIndex.x) && ((valuesY[i]*8.0) < thisIndex.y))
				{
					tmpBufX = (double *)realloc(tmpBufX,sizeof(double)*(j+1));					
					tmpBufY = (double *)realloc(tmpBufY,sizeof(double)*(j+1));					
					tmpBufX[j] = valuesX[i];
					tmpBufY[j] = valuesY[i];
					lostIndex = i;
					for(k=i+1;k<count;k++,i++)		//delete the particle going out of the chare's boundaries.
					{
						valuesX[i] = valuesX[k];
						valuesY[i] = valuesY[k];
					}
					i = lostIndex;
					count--;				//decrement count. 
					j++;
				}	
			}
			if(j!=0)			
				thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, (thisIndex.y -1 + 8)%8)].Ghost(tmpBufX, tmpBufY,j);
			else
				thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, (thisIndex.y -1 + 8)%8)].Ghost(blankGhostX, blankGhostY,0);
	


			j = 0;
			for(i = 0;i<count;i++)					//Case 2: Send WW
			{
				if(((valuesX[i]*8.0) < thisIndex.x) && ((valuesY[i]*8.0) >= thisIndex.y) && ((valuesY[i]*8.0)<= (thisIndex.y + 1)))
				{
					tmpBufX = (double *)realloc(tmpBufX,sizeof(double)*(j+1));					
					tmpBufY = (double *)realloc(tmpBufY,sizeof(double)*(j+1));						
					tmpBufX[j] = valuesX[i];		//rewrite to temporary buffer
					tmpBufY[j] = valuesY[i];
					lostIndex = i;
					for(k=i+1;k<count;k++,i++)		
					{
						valuesX[i] = valuesX[k];
						valuesY[i] = valuesY[k];
					}
					i = lostIndex;
					count--;				
					j++;
				}	
			}
			if(j!=0)
				thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, thisIndex.y)].Ghost(tmpBufX, tmpBufY,j);
			else 
				thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, thisIndex.y)].Ghost(blankGhostX, blankGhostY,0);
	


			j = 0;
			for(i = 0;i<count;i++)					//Case 3: Send SW
			{
				if(((valuesX[i]*8.0) < thisIndex.x) && ((valuesY[i]*8.0) > (thisIndex.y + 1)))
				{
					tmpBufX = (double *)realloc(tmpBufX,sizeof(double)*(j+1));					
					tmpBufY = (double *)realloc(tmpBufY,sizeof(double)*(j+1));						
					tmpBufX[j] = valuesX[i];
					tmpBufY[j] = valuesY[i];
					lostIndex = i;
					for(k=i+1;k<count;k++,i++)		
					{
						valuesX[i] = valuesX[k];
						valuesY[i] = valuesY[k];
					}
					i = lostIndex;
					count--;				
					j++;
				}	
			
			}
			if(j!=0)
				thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, (thisIndex.y + 1)%8)].Ghost(tmpBufX, tmpBufY,j);
			else
				thisProxy[CkArrayIndex2D((thisIndex.x -1 + 8)%8, (thisIndex.y + 1)%8)].Ghost(blankGhostX, blankGhostY,0);
		


			j = 0;
			for(i = 0;i<count;i++)					//Case 4: Send SS
			{
				if(((valuesX[i]*8.0)>=thisIndex.x) && ((valuesX[i]*8.0)<=(thisIndex.x + 1)) && ((valuesY[i]*8.0)>(thisIndex.y+ 1)))
				{
					tmpBufX = (double *)realloc(tmpBufX,sizeof(double)*(j+1));					
					tmpBufY = (double *)realloc(tmpBufY,sizeof(double)*(j+1));						
					tmpBufX[j] = valuesX[i];
					tmpBufY[j] = valuesY[i];
					lostIndex = i;
					for(k=i+1;k<count;k++,i++)		
					{
						valuesX[i] = valuesX[k];
						valuesY[i] = valuesY[k];
					}
					i = lostIndex;
					count--;				 
					j++;
				}	
			}
			if(j!=0)
				thisProxy[CkArrayIndex2D(thisIndex.x, (thisIndex.y + 1)%8)].Ghost(tmpBufX, tmpBufY,j);
			else
				thisProxy[CkArrayIndex2D(thisIndex.x, (thisIndex.y + 1)%8)].Ghost(blankGhostX, blankGhostY,0);



			j = 0;
			for(i = 0;i<count;i++)					//Case 5: Send SE
			{
				if(((valuesX[i]*8.0) > (thisIndex.x + 1)) && ((valuesY[i]*8.0) > (thisIndex.y + 1)))
				{
					tmpBufX = (double *)realloc(tmpBufX,sizeof(double)*(j+1));					
					tmpBufY = (double *)realloc(tmpBufY,sizeof(double)*(j+1));						
					tmpBufX[j] = valuesX[i];
					tmpBufY[j] = valuesY[i];
					lostIndex = i;
					for(k=i+1;k<count;k++,i++)		
					{
						valuesX[i] = valuesX[k];
						valuesY[i] = valuesY[k];
					}
					i = lostIndex;
					count--;				 
					j++;
				}	
			}
			if(j!=0)
				thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, (thisIndex.y + 1)%8)].Ghost(tmpBufX, tmpBufY,j);
			else
				thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, (thisIndex.y + 1)%8)].Ghost(blankGhostX, blankGhostY,0);


			
			j = 0;
			for(i = 0;i<count;i++)					//Case 6: Send EE
			{
				if(((valuesX[i]*8.0)>(thisIndex.x + 1))&&((valuesY[i]*8.0) <= (thisIndex.y + 1))&&((valuesY[i]*8.0)>= thisIndex.y))
				{
					tmpBufX = (double *)realloc(tmpBufX,sizeof(double)*(j+1));					
					tmpBufY = (double *)realloc(tmpBufY,sizeof(double)*(j+1));						
					tmpBufX[j] = valuesX[i];
					tmpBufY[j] = valuesY[i];
					lostIndex = i;
					for(k=i+1;k<count;k++,i++)		
					{
						valuesX[i] = valuesX[k];
						valuesY[i] = valuesY[k];
					}
					i = lostIndex;
					count--;				 
					j++;
				}	
			}
			if(j!=0)
				thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, thisIndex.y)].Ghost(tmpBufX, tmpBufY,j);
			else
				thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, thisIndex.y)].Ghost(blankGhostX, blankGhostY,0);
	

			
			j = 0;
			for(i = 0;i<count;i++)					//Case 7: Send NE
			{
				if(((valuesX[i]*8.0) > (thisIndex.x + 1)) && ((valuesY[i]*8.0) < thisIndex.y))
				{
					tmpBufX = (double *)realloc(tmpBufX,sizeof(double)*(j+1));					
					tmpBufY = (double *)realloc(tmpBufY,sizeof(double)*(j+1));						
					tmpBufX[j] = valuesX[i];
					tmpBufY[j] = valuesY[i];
					lostIndex = i;
					for(k=i+1;k<count;k++,i++)		
					{
						valuesX[i] = valuesX[k];
						valuesY[i] = valuesY[k];
					}
					i = lostIndex;
					count--;				 
					j++;
				}	 
			}
			if(j!=0)
				thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, (thisIndex.y -1 + 8)%8)].Ghost(tmpBufX, tmpBufY,j);
			else 
				thisProxy[CkArrayIndex2D((thisIndex.x + 1)%8, (thisIndex.y -1 + 8)%8)].Ghost(blankGhostX, blankGhostY,0);
	


			j = 0;
			for(i = 0;i<count;i++)					//Case 8: Send NN
			{
				if(((valuesX[i]*8.0) >= thisIndex.x) && ((valuesX[i]*8.0)<= (thisIndex.x + 1)) && ((valuesY[i]*8.0) < thisIndex.y))
				{
					tmpBufX = (double *)realloc(tmpBufX,sizeof(double)*(j+1));					
					tmpBufY = (double *)realloc(tmpBufY,sizeof(double)*(j+1));						
					tmpBufX[j] = valuesX[i];
					tmpBufY[j] = valuesY[i];
					lostIndex = i;
					for(k=i+1;k<count;k++,i++)		
					{
						valuesX[i] = valuesX[k];
						valuesY[i] = valuesY[k];
					}
					i = lostIndex;
					count--;				 
					j++;
				}	
			}
			if(j!=0)
				thisProxy[CkArrayIndex2D(thisIndex.x, (thisIndex.y -1 + 8)%8)].Ghost(tmpBufX, tmpBufY,j);
			else
				thisProxy[CkArrayIndex2D(thisIndex.x, (thisIndex.y -1 + 8)%8)].Ghost(blankGhostX, blankGhostY,0);
			
			free(tmpBufX);						//Free the memory assigned to buffers.
			free(tmpBufY);
			free(blankGhostX);
			free(blankGhostY);
		};								/****free only deallocates the memory assigned. It does 										     not assign a NULL pointer****IMPORTANT to 											     avoid Seg Faults!
										http://www.cplusplus.com/reference/clibrary/cstdlib/free/
										****/
	public:		
		points(CkMigrateMessage *msg)
		{};
		
		points()
		{
			//usesAtSync = CmiTrue;					//Must be set to CmiTrue to make AtSync work. 
			count = 0;						//Number of points stored by each chare.(initially 0).
			ghostCount = 0;						//How many ghost messages have been received by the chare.
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
			shiftPoints();		
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
		
		void Ghost(double *tmpBufX, double *tmpBufY, int pointsReceived)	//Receive and process ghosts.
		{
			ghostCount++;
			valuesX = (double *)realloc(valuesX,sizeof(double)*(count + pointsReceived));//NOTE (count+pointsReceived)!! 
			valuesY = (double *)realloc(valuesY,sizeof(double)*(count + pointsReceived));
			int i;
			for(i=count;i<(pointsReceived + count);i++)
			{
				valuesX[i] = tmpBufX[i - count];
				valuesY[i] = tmpBufY[i - count];
			}
			count = count + pointsReceived;				//Now each chare contains these many points.
			if(ghostCount == 8)
			{			
				ghostCount = 0;					//Re-initialize this variable to zero.
				mainProxy.checkIn(count);
			}
		};

		/*void balanceLoad()
		{
			AtSync();	
		};
		
		void ResumeFromSync()
		{
			shiftPoints();
		};
		
		void pup(PUP::er &p) 
		{
			CBase_points::pup(p);
			p|count;
			p|ghostCount;*/
};

			
#include"load_balancing.def.h"

			
