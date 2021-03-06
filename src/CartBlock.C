//
// This file is part of the Tioga software library
//
// Tioga  is a tool for overset grid assembly on parallel distributed systems
// Copyright (C) 2015 Jay Sitaraman
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with this library; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
#include "codetypes.h"
#include "CartBlock.h"
#include "CartGrid.h"
#include <assert.h>
extern "C" {
  void deallocateLinkList(DONORLIST *temp);
  void deallocateLinkList2(INTEGERLIST *temp);
  void deallocateLinkList3(INTEGERLIST2 *temp);
  void deallocateLinkList4(INTERPLIST2 *temp);
  void insertInList(DONORLIST **donorList,DONORLIST *temp1);
  void get_amr_index_xyz( int nq,int i,int j,int k,
			  int pBasis,
			  int nX,int nY,int nZ,
			  int nf,
			  double xlo[3],double dx[3],
			  double qnodes[],
			  int* index, double* xyz);
    void amr_index_to_ijklmn(int pBasis,int nX,int nY,int nZ, int nf, int nq,
			     int index, int* ijklmn);
    int checkHoleMap(double *x,int *nx,int *sam,double *extents);
    //void writeqnode_(int *myid,double *qnodein,int *qnodesize);
}
void CartBlock::getInterpolatedData(int *nints,int *nreals,int **intData,
				    double **realData,
				    int nvar)
{
  int i,n;
  int ploc;
  double *xtmp;
  int *index;
  double *qq;
  int *tmpint;
  double *tmpreal;
  int icount,dcount;
  int nintold,nrealold;
  int interpCount=0;
  double weight;
  listptr=interpList;
  while(listptr!=NULL)
    {
      interpCount++;
      listptr=listptr->next;
    }
  if (interpCount > 0) 
    {
      nintold=(*nints);
      nrealold=(*nreals);
      if (nintold > 0) {
      tmpint=(int *)malloc(sizeof(int)*3*(*nints));
      tmpreal=(double *)malloc(sizeof(double)*(*nreals));
      for(i=0;i<(*nints)*3;i++) tmpint[i]=(*intData)[i];
      for(i=0;i<(*nreals);i++) tmpreal[i]=(*realData)[i];
      }
      (*nints)+=interpCount;
      (*nreals)+=(interpCount*nvar);
      (*intData)=(int *)malloc(sizeof(int)*3*(*nints));
      (*realData)=(double *)malloc(sizeof(double)*(*nreals));
      if (nintold > 0) {
      for(i=0;i<nintold*3;i++) (*intData)[i]=tmpint[i];
      for(i=0;i<nrealold;i++) (*realData)[i]=tmpreal[i];      
      free(tmpint);
      free(tmpreal);
      }
      listptr=interpList;
      icount=3*nintold;
      dcount=nrealold;
      xtmp=(double *)malloc(sizeof(double)*p3*3);
      index=(int *)malloc(sizeof(int)*p3);
      ploc=(pdegree)*(pdegree+1)/2;
      qq=(double *)malloc(sizeof(double)*nvar);
      while(listptr!=NULL)
	{
	  get_amr_index_xyz(qstride,listptr->inode[0],listptr->inode[1],listptr->inode[2],
			    pdegree,dims[0],dims[1],dims[2],nf,
			    xlo,dx,&qnode[ploc],index,xtmp);
	  assert(listptr->receptorInfo[0]<80);
	  (*intData)[icount++]=listptr->receptorInfo[0];
	  (*intData)[icount++]=-1;
	  (*intData)[icount++]=listptr->receptorInfo[1];	  
	  for(n=0;n<nvar;n++)
           {
            qq[n]=0;
	    for(i=0;i<p3;i++)
	      {
		weight=listptr->weights[i];
		qq[n]+=q[index[i]+d3nf*n]*weight;
	      }
            }
          //writeqnode_(&myid,qq,&nvar);
	  for(n=0;n<nvar;n++)
	    (*realData)[dcount++]=qq[n];
	  listptr=listptr->next;
	}
      free(xtmp);
      free(index);
      free(qq);
    }
}


void CartBlock::update(double *qval, int index,int nq)
{
  int i;
  for(i=0;i<nq;i++)
    q[index+d3nf*i]=qval[i];
}

  
void CartBlock::preprocess(CartGrid *cg)
  {
    int nfrac;
    for(int n=0;n<3;n++) xlo[n]=cg->xlo[3*global_id+n];
    for(int n=0;n<3;n++) dx[n]=cg->dx[3*global_id+n];
    dims[0]=cg->ihi[3*global_id]  -cg->ilo[3*global_id  ]+1;
    dims[1]=cg->ihi[3*global_id+1]-cg->ilo[3*global_id+1]+1;
    dims[2]=cg->ihi[3*global_id+2]-cg->ilo[3*global_id+2]+1;
    pdegree=cg->porder[global_id];
    p3=(pdegree+1)*(pdegree+1)*(pdegree+1);
    nf=cg->nf;
    myid=cg->myid;
    qstride=cg->qstride;
    donor_frac=cg->donor_frac;
    qnode=cg->qnode;
    d1=dims[0];
    d2=dims[0]*dims[1];
    d3=d2*dims[2];
    d3nf=(dims[0]+2*nf)*(dims[1]+2*nf)*(dims[2]+2*nf);
    ndof=d3*p3;
  };

void CartBlock::initializeLists(void)
{
 donorList=(DONORLIST **)malloc(sizeof(DONORLIST *)*ndof);
 for(int i=0;i<ndof;i++) donorList[i]=NULL;
}

void CartBlock::clearLists(void)
{
  int i;
  if (donorList) {
  for(i=0;i<ndof;i++) { deallocateLinkList(donorList[i]); donorList[i]=NULL;}
  free(donorList);
  }
  deallocateLinkList4(interpList);
  interpList=NULL;
}


void CartBlock::insertInInterpList(int procid,int remoteid,double *xtmp)
{
  int i,n;
  int ix[3];
  double *rst;
  rst=(double *)malloc(sizeof(double)*3);
  if (interpList==NULL) 
    {
      interpList=(INTERPLIST2 *)malloc(sizeof(INTERPLIST2));
      listptr=interpList;
    }
  else
    {
      listptr->next=(INTERPLIST2 *)malloc(sizeof(INTERPLIST2));
      listptr=listptr->next;
    }
  listptr->next=NULL;
  listptr->inode=NULL;
  listptr->weights=NULL;
  listptr->receptorInfo[0]=procid;
  listptr->receptorInfo[1]=remoteid;
  for(n=0;n<3;n++)
    {
      ix[n]=(xtmp[n]-xlo[n])/dx[n];
      rst[n]=(xtmp[n]-xlo[n]-ix[n]*dx[n])/dx[n];
      // if (!(ix[n] >=0 && ix[n] < dims[n]) && myid==77) {
      //  tracei(procid);
      //  tracei(global_id);
      //  tracei(local_id);
      //  tracei(remoteid);
      //  tracei(myid);
      //  traced(xtmp[0]);
      //  traced(xtmp[1]);
      //  traced(xtmp[2]);
      //  traced(xlo[0]);
      //  traced(xlo[1]);
      //  traced(xlo[2]);
      //  traced(dx[0]);
      //  traced(dx[1]);
      //  traced(dx[2]);
      //  tracei(ix[n]);
      //  tracei(n);
      //  tracei(dims[n]);
      //  printf("--------------------------\n");
      // }
     assert((ix[n] >=0 && ix[n] < dims[n]));
    }
  listptr->nweights=(pdegree+1)*(pdegree+1)*(pdegree+1);
  listptr->weights=(double *)malloc(sizeof(double)*listptr->nweights);
  listptr->inode=(int *)malloc(sizeof(int)*3);
  listptr->inode[0]=ix[0];
  listptr->inode[1]=ix[1];
  listptr->inode[2]=ix[2];
  donor_frac(&pdegree,rst,&(listptr->nweights),(listptr->weights));  
  free(rst);
}
  
void CartBlock::insertInDonorList(int senderid,int index,int meshtagdonor,int remoteid,double cellRes)
{
  DONORLIST *temp1;
  int ijklmn[6];
  int pointid;
  temp1=(DONORLIST *)malloc(sizeof(DONORLIST));
  amr_index_to_ijklmn(pdegree,dims[0],dims[1],dims[2],nf,qstride,index,ijklmn);
  //pointid=ijklmn[5]*(pdegree+1)*(pdegree+1)*d3+
  //        ijklmn[4]*(pdegree+1)*d3+
  //        ijklmn[3]*d3+
  //        ijklmn[2]*d2+
  //        ijklmn[1]*d1+
  //        ijklmn[0];
  pointid=(ijklmn[2]*d2+ijklmn[1]*d1+ijklmn[0])*p3+
           ijklmn[5]*(pdegree+1)*(pdegree+1)+
           ijklmn[4]*(pdegree+1)+ijklmn[3];
  if (!(pointid >= 0 && pointid < ndof)) {
    tracei(index);
    tracei(nf);
    tracei(pdegree);
    tracei(dims[0]);
    tracei(dims[1]);
    tracei(dims[2]);
    tracei(qstride);
    printf("%d %d %d %d %d %d\n",ijklmn[0],ijklmn[1],ijklmn[2],
	   ijklmn[3],ijklmn[4],ijklmn[5]);
  }
  assert((pointid >= 0 && pointid < ndof));
    
  temp1->donorData[0]=senderid;
  temp1->donorData[1]=meshtagdonor;
  temp1->donorData[2]=remoteid;
  temp1->donorRes=cellRes;
  temp1->cancel=0;
  insertInList(&(donorList[pointid]),temp1);
}

void CartBlock::processDonors(HOLEMAP *holemap, int nmesh)
{
  int i,j,k,l,m,n,h,p;
  int ibcount,idof,meshtagdonor,icount;
  DONORLIST *temp;
  int *iflag;
  int holeFlag;
  double *xtmp;
  int *index;
  int ploc,ibindex;
  //FILE*fp;
  char fname[80];
  char qstr[2];
  char intstring[7];

  //sprintf(intstring,"%d",100000+myid);
  //sprintf(fname,"fringes_%s.dat",&(intstring[1]));
  //if (local_id==0) 
  //  {
  //    fp=fopen(fname,"w");
  //  }
  //else
  //  {
  //    fp=fopen(fname,"a");
  //  }  

  //
  // first mark hole points
  //
  iflag=(int *)malloc(sizeof(int)*nmesh);
  index=(int *)malloc(sizeof(int)*p3);
  xtmp=(double *)malloc(sizeof(double)*p3*3);
  //
  ibcount=-1;
  idof=-1;
  //for(i=0;i<(dims[0]+2*nf)*(dims[1]+2*nf)*(dims[2]+2*nf);i++) ibl[i]=1;
  ploc=pdegree*(pdegree+1)/2;

  for(k=0;k<dims[2];k++)
    for(j=0;j<dims[1];j++)
      for(i=0;i<dims[0];i++)
	{
	  ibcount++;
	  get_amr_index_xyz(qstride,i,j,k,pdegree,dims[0],dims[1],dims[2],nf,
			    xlo,dx,&qnode[ploc],index,xtmp);
          holeFlag=1;
          idof=ibcount*p3-1;
	  for(p=0;p<p3 && holeFlag;p++)
	    {
	      idof++;
	      if (donorList[idof]==NULL)
		{
		  for(h=0;h<nmesh;h++)
		    if (holemap[h].existWall)
		      {
			if (checkHoleMap(&xtmp[3*p],holemap[h].nx,holemap[h].sam,holemap[h].extents))
			  {
	                    ibindex=(k+nf)*(dims[1]+2*nf)*(dims[0]+2*nf)+(j+nf)*(dims[0]+2*nf)+i+nf;
			    ibl[ibindex]=0;
                            holeFlag=0;
			    break;
			  }
		      }
		}
	      else
		{
		  temp=donorList[idof];
		  for(h=0;h<nmesh;h++) iflag[h]=0;
		  while(temp!=NULL) 
		    {
		      meshtagdonor=temp->donorData[1]-BASE;
		      iflag[meshtagdonor]=1;
		      temp=temp->next;
		    }
		  for(h=0;h<nmesh;h++)
		    {
		      if (holemap[h].existWall)
			{
			  if (!iflag[h])
			    if (checkHoleMap(&xtmp[3*p],holemap[h].nx,holemap[h].sam,holemap[h].extents))
			      {
	                        ibindex=(k+nf)*(dims[1]+2*nf)*(dims[0]+2*nf)+(j+nf)*(dims[0]+2*nf)+i+nf;
				ibl[ibindex]=0;
                                holeFlag=0;
				break;
			      }
			}
		    }
		}
	    }
	}
  ibcount=-1;
  idof=-1;
  for(k=0;k<dims[2];k++)
    for(j=0;j<dims[1];j++)
      for(i=0;i<dims[0];i++)
	{
	  ibcount++;
          ibindex=(k+nf)*(dims[1]+2*nf)*(dims[0]+2*nf)+(j+nf)*(dims[0]+2*nf)+i+nf;
          get_amr_index_xyz(qstride,i,j,k,pdegree,dims[0],dims[1],dims[2],nf,
                            xlo,dx,&qnode[ploc],index,xtmp);
	  if (ibl[ibindex]==0) 
	    {
              idof=ibcount*p3-1;
	      for(p=0;p<p3;p++)
		{
		  idof++;
		  if (donorList[idof]!=NULL)
		    {
		      temp=donorList[idof];
		      while(temp!=NULL)
			{
			  temp->cancel=1;
			  temp=temp->next;
			}
		    }
		}

	    }
	  else
	    {
	      icount=0;
              idof=ibcount*p3-1;
	      for(p=0;p<p3;p++)
		{
		  idof++;
		  if (donorList[idof]!=NULL)
		    {
		      temp=donorList[idof];
		      while(temp!=NULL)
			{
			  if (temp->donorRes < BIGVALUE)
			    {
			      icount++;
			      break;
			    }
			  temp=temp->next;
			}
		    }
		}
	      if (icount==p3) 
		{
		  ibl[ibindex]=-1;
                  //for(p=0;p<p3;p++)
	          // fprintf(fp,"%f %f %f\n",xtmp[3*p],xtmp[3*p+1],xtmp[3*p+2]);
		}
	      else
		{
                  idof=ibcount*p3-1;
		  for(p=0;p<p3;p++)
		    {
		      idof++;
		      if (donorList[idof]!=NULL)
			{
			  temp=donorList[idof];
			  while(temp!=NULL)
			    {
			      temp->cancel=1;
			      temp=temp->next;
			    }
			}
		    } 
		}
	    }
	}
  // fclose(fp);
}
			      

void CartBlock::getCancellationData(int *cancelledData, int *ncancel)
{
  int i,j,k,p,m;
  int idof;
  DONORLIST *temp;
  idof=-1;
  m=0;
  *ncancel=0;
  for(k=0;k<dims[2];k++)
    for(j=0;j<dims[1];j++)
      for(i=0;i<dims[0];i++)
	for(p=0;p<p3;p++)
	  {
	    idof++;
	    if (donorList[idof]!=NULL)
	      {
		temp=donorList[idof];
		while(temp!=NULL)
		  {
		    if (temp->cancel==1) {
		      (*ncancel)++;
		      cancelledData[m++]=temp->donorData[0];
		      cancelledData[m++]=1;
		      cancelledData[m++]=temp->donorData[2];
		    }
	            temp=temp->next;
		  }
	      }
	  }
}


void CartBlock::writeCellFile(int bid)
{
  int ibmin,ibmax;
  char fname[80];
  char qstr[2];
  char intstring[7];
  char hash,c;
  int i,n,j,k,ibindex;
  int bodytag;
  FILE *fp;
  int ba,id;
  int nvert;
  int nnodes,ncells;
  int dd1,dd2;
  
  ibmin=30000000;
  ibmax=-30000000;
  nnodes=(dims[1]+1)*(dims[0]+1)*(dims[2]+1);
  ncells=dims[0]*dims[1]*dims[2];
  sprintf(intstring,"%d",100000+myid);
  sprintf(fname,"cart_cell%s.dat",&(intstring[1]));
  if (bid==0) 
    {
      fp=fopen(fname,"w");
    }
  else
    {
      fp=fopen(fname,"a");
    }
  if (bid==0) {
  fprintf(fp,"TITLE =\"Tioga output\"\n");
  fprintf(fp,"VARIABLES=\"X\",\"Y\",\"Z\",\"IBLANK_CELL\" ");
  fprintf(fp,"\n");
  }
  fprintf(fp,"ZONE T=\"VOL_MIXED\",N=%d E=%d ET=BRICK, F=FEBLOCK\n",nnodes,
	  ncells);
  fprintf(fp,"VARLOCATION =  (1=NODAL, 2=NODAL, 3=NODAL, 4=CELLCENTERED)\n");

  for(k=0;k<dims[2]+1;k++)
    for(j=0;j<dims[1]+1;j++)
      for(i=0;i<dims[0]+1;i++)
	fprintf(fp,"%lf\n",xlo[0]+dx[0]*i);
  for(k=0;k<dims[2]+1;k++)
    for(j=0;j<dims[1]+1;j++)
      for(i=0;i<dims[0]+1;i++)
	fprintf(fp,"%lf\n",xlo[1]+dx[1]*j);
  for(k=0;k<dims[2]+1;k++)
    for(j=0;j<dims[1]+1;j++)
      for(i=0;i<dims[0]+1;i++)
	fprintf(fp,"%lf\n",xlo[2]+dx[2]*k);
		
  for(k=0;k<dims[2];k++)
    for(j=0;j<dims[1];j++)
      for(i=0;i<dims[0];i++)
	{
	  ibindex=(k+nf)*(dims[1]+2*nf)*(dims[0]+2*nf)+
	    (j+nf)*(dims[0]+2*nf)+(i+nf);
          ibmin=min(ibmin,ibl[ibindex]);
          ibmax=max(ibmax,ibl[ibindex]);
	  fprintf(fp,"%d\n", ibl[ibindex]);
	}

  //printf("proc %d , block %d, ibmin/ibmax=%d %d\n",myid,bid,ibmin,ibmax);
  id=0;
  dd1=(dims[0]+1);
  dd2=dd1*(dims[1]+1);
  for(k=0;k<dims[2];k++)
    for(j=0;j<dims[1];j++)
      for(i=0;i<dims[0];i++)
        {
	  id=k*dd2+j*dd1+i+1;
          fprintf(fp,"%d %d %d %d %d %d %d %d\n",
		 id,
		 id+1,
		 id+1+dd1,
		 id+dd1,
		 id+dd2,
		 id+1+dd2,
		 id+1+dd1+dd2,
		 id+dd1+dd2);
	}
                                               
  fclose(fp);
  return;
}
