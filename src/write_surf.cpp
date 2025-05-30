/* ----------------------------------------------------------------------
   SPARTA - Stochastic PArallel Rarefied-gas Time-accurate Analyzer
   http://sparta.github.io
   Steve Plimpton, sjplimp@gmail.com, Michael Gallis, magalli@sandia.gov
   Sandia National Laboratories

   Copyright (2014) Sandia Corporation.  Under the terms of Contract
   DE-AC04-94AL85000 with Sandia Corporation, the U.S. Government retains
   certain rights in this software.  This software is distributed under
   the GNU General Public License.

   See the README file in the top-level SPARTA directory.
------------------------------------------------------------------------- */

#include "mpi.h"
#include "spatype.h"
#include "string.h"
#include "stdlib.h"
#include "write_surf.h"
#include "surf.h"
#include "update.h"
#include "comm.h"
#include "domain.h"
#include "memory.h"
#include "error.h"

using namespace SPARTA_NS;

#define MAXLINE 256

/* ---------------------------------------------------------------------- */

WriteSurf::WriteSurf(SPARTA *sparta) : Pointers(sparta)
{
  statflag = 1;
}

/* ---------------------------------------------------------------------- */

void WriteSurf::command(int narg, char **arg)
{
  if (!surf->exist)
    error->all(FLERR,"Cannot write surf when surfs do not exist");

  if (narg < 1) error->all(FLERR,"Illegal write_surf command");

  me = comm->me;
  nprocs = comm->nprocs;
  dim = domain->dimension;

  if (statflag && me == 0)
    if (screen) fprintf(screen,"Writing surface file ...\n");

  // if filename contains a "*", replace with current timestep

  char *ptr;
  int n = strlen(arg[0]) + 16;
  char *file = new char[n];

  if ((ptr = strchr(arg[0],'*'))) {
    *ptr = '\0';
    sprintf(file,"%s" BIGINT_FORMAT "%s",arg[0],update->ntimestep,ptr+1);
  } else strcpy(file,arg[0]);

  // check for multiproc output

  if (strchr(arg[0],'%')) multiproc = nprocs;
  else multiproc = 0;

  // optional args

  pointflag = 1;
  typeflag = 0;
  ncustom = 0;
  index_custom = NULL;
  type_custom = NULL;
  size_custom = NULL;
  nvalues_custom = 0;

  if (multiproc) {
    nclusterprocs = 1;
    filewriter = 1;
    fileproc = me;
    icluster = me;
  } else {
    nclusterprocs = nprocs;
    filewriter = 0;
    if (me == 0) filewriter = 1;
    fileproc = 0;
  }

  int iarg = 1;
  while (iarg < narg) {
    if (strcmp(arg[iarg],"points") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal write_surf command");
      if (strcmp(arg[iarg+1],"yes") == 0) pointflag = 1;
      else if (strcmp(arg[iarg+1],"no") == 0) pointflag = 0;
      else error->all(FLERR,"Illegal write_surf command");
      iarg += 2;

    } else if (strcmp(arg[iarg],"type") == 0) {
      typeflag = 1;
      iarg++;

    } else if (strcmp(arg[iarg],"custom") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Invalid write_surf command");
      if (surf->implicit)
        error->all(FLERR,"Cannot use write_surf custom with implicit surfs");

      memory->grow(index_custom,ncustom+1,"writesurf:index_custom");
      memory->grow(type_custom,ncustom+1,"writesurf:type_custom");
      memory->grow(size_custom,ncustom+1,"writesurf:size_custom");

      int index = surf->find_custom(arg[iarg+1]);
      if (index < 0) error->all(FLERR,"Write_surf custom name does not exist");
      index_custom[ncustom] = index;
      type_custom[ncustom] = surf->etype[index];
      size_custom[ncustom] = surf->esize[index];
      if (size_custom[ncustom] == 0) nvalues_custom++;
      else nvalues_custom += size_custom[ncustom];
      ncustom++;

      iarg += 2;

    } else if (strcmp(arg[iarg],"fileper") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal write_sirf command");
      if (!multiproc)
        error->all(FLERR,"Cannot use write_surf fileper "
                   "without % in surface file name");
      int nper = atoi(arg[iarg+1]);
      if (nper <= 0) error->all(FLERR,"Illegal write_surf command");

      multiproc = nprocs/nper;
      if (nprocs % nper) multiproc++;
      fileproc = me/nper * nper;
      int fileprocnext = MIN(fileproc+nper,nprocs);
      nclusterprocs = fileprocnext - fileproc;
      if (me == fileproc) filewriter = 1;
      else filewriter = 0;
      icluster = fileproc/nper;
      iarg += 2;

    } else if (strcmp(arg[iarg],"nfile") == 0) {
      if (iarg+2 > narg) error->all(FLERR,"Illegal write_surf command");
      if (!multiproc)
        error->all(FLERR,"Cannot use write_surf nfile "
                   "without % in surface file name");
      int nfile = atoi(arg[iarg+1]);
      if (nfile <= 0) error->all(FLERR,"Illegal write_surf command");
      nfile = MIN(nfile,nprocs);

      multiproc = nfile;
      icluster = static_cast<int> ((bigint) me * nfile/nprocs);
      fileproc = static_cast<int> ((bigint) icluster * nprocs/nfile);
      int fcluster = static_cast<int> ((bigint) fileproc * nfile/nprocs);
      if (fcluster < icluster) fileproc++;
      int fileprocnext =
        static_cast<int> ((bigint) (icluster+1) * nprocs/nfile);
      fcluster = static_cast<int> ((bigint) fileprocnext * nfile/nprocs);
      if (fcluster < icluster+1) fileprocnext++;
      nclusterprocs = fileprocnext - fileproc;
      if (me == fileproc) filewriter = 1;
      else filewriter = 0;
      iarg += 2;

    } else error->all(FLERR,"Illegal write_surf command");
  }

  // for custom data with non-distributed surfs,
  //   check if custom data needs to be spread to owned surfs

  if (ncustom && !surf->distributed) {
    for (int ic = 0; ic < ncustom; ic++) {
      int index = index_custom[ic];
      if (surf->estatus[index] == 0)
        surf->spread_custom(index);
    }
  }

  // write file(s)

  MPI_Barrier(world);
  double time1 = MPI_Wtime();

  write_file(file);
  delete [] file;

  MPI_Barrier(world);
  double time2 = MPI_Wtime();

  // clean up

  memory->destroy(index_custom);
  memory->destroy(type_custom);
  memory->destroy(size_custom);

  // output stats on surfs written out
  // if called from ReadSurf or ReadIsurf, statflag is unset by caller

  if (!statflag) return;

  double time_total = time2-time1;

  if (me == 0) {
    if (screen) {
      fprintf(screen,"  surf elements = " BIGINT_FORMAT "\n",surf->nsurf);
      fprintf(screen,"  CPU time = %g secs\n",time_total);
    }
    if (logfile) {
      fprintf(logfile,"  surf elements = " BIGINT_FORMAT "\n",surf->nsurf);
      fprintf(logfile,"  CPU time = %g secs\n",time_total);
    }
  }
}

/* ---------------------------------------------------------------------- */

void WriteSurf::write_file(char *file)
{
  if (surf->distributed) {
    if (pointflag) write_file_distributed_points(file);
    else write_file_distributed_nopoints(file);
  } else {
    if (pointflag) write_file_all_points(file);
    else write_file_all_nopoints(file);
  }
}

/* ----------------------------------------------------------------------
   write surf file to single or multiple files with Points section
   each proc has copy of all surfs
------------------------------------------------------------------------- */

void WriteSurf::write_file_all_points(char *file)
{
  // if multiproc, write base file

  if (multiproc && me == 0) write_base(file);

  // open single surface file or multi file for multiproc

  if (filewriter) open(file);

  // nmine = # of surfs I contribute to this file
  // my fraction of all surfs

  int first = static_cast<int> (1.0*me/nprocs * surf->nlocal);
  int next = static_cast<int> (1.0*(me+1)/nprocs * surf->nlocal);
  int nmine = next - first;

  // write header info

  write_header(nmine);

  if (!filewriter) return;

  // istart/istop = lower/upper bound of surfs written by this proc

  int istart = static_cast<int> (1.0*me/nprocs * surf->nlocal);
  int istop = static_cast<int> (1.0*(me+nclusterprocs)/nprocs * surf->nlocal);

  // points

  fprintf(fp,"\nPoints\n\n");

  if (dim == 2) {
    Surf::Line *lines = surf->lines;
    bigint m = 0;
    for (int i = istart; i < istop; i++) {
      fprintf(fp,BIGINT_FORMAT " %20.15g %20.15g\n",
              m+1,lines[i].p1[0],lines[i].p1[1]);
      fprintf(fp,BIGINT_FORMAT " %20.15g %20.15g\n",
              m+2,lines[i].p2[0],lines[i].p2[1]);
      m += 2;
    }
  } else {
    Surf::Tri *tris = surf->tris;
    bigint m = 0;
    for (int i = istart; i < istop; i++) {
      fprintf(fp,BIGINT_FORMAT " %20.15g %20.15g %20.15g\n",
              m+1,tris[i].p1[0],tris[i].p1[1],tris[i].p1[2]);
      fprintf(fp,BIGINT_FORMAT " %20.15g %20.15g %20.15g\n",
              m+2,tris[i].p2[0],tris[i].p2[1],tris[i].p2[2]);
      fprintf(fp,BIGINT_FORMAT " %20.15g %20.15g %20.15g\n",
              m+3,tris[i].p3[0],tris[i].p3[1],tris[i].p3[2]);
      m += 3;
    }
  }

  // lines

  if (dim == 2) {
    Surf::Line *lines = surf->lines;
    fprintf(fp,"\nLines\n\n");
    bigint m = 0;

    if (typeflag)
      for (int i = istart; i < istop; i++) {
	fprintf(fp,SURFINT_FORMAT " %d " BIGINT_FORMAT " " BIGINT_FORMAT,
		lines[i].id,lines[i].type,m+1,m+2);
	if (ncustom) write_custom_all(i);
	fprintf(fp,"\n");
	m += 2;
      }
    else
      for (int i = istart; i < istop; i++) {
	fprintf(fp,SURFINT_FORMAT " " BIGINT_FORMAT " " BIGINT_FORMAT,
		lines[i].id,m+1,m+2);
	if (ncustom) write_custom_all(i);
	fprintf(fp,"\n");
	m += 2;
      }
  }

  // triangles

  if (dim == 3) {
    Surf::Tri *tris = surf->tris;
    fprintf(fp,"\nTriangles\n\n");
    bigint m = 0;

    if (typeflag)
      for (int i = istart; i < istop; i++) {
	fprintf(fp,SURFINT_FORMAT " %d " BIGINT_FORMAT " "
		BIGINT_FORMAT " " BIGINT_FORMAT,
		tris[i].id,tris[i].type,m+1,m+2,m+3);
	if (ncustom) write_custom_all(i);
	fprintf(fp,"\n");
	m += 3;
      }
    else
      for (int i = istart; i < istop; i++) {
	fprintf(fp,SURFINT_FORMAT " " BIGINT_FORMAT " "
		BIGINT_FORMAT " " BIGINT_FORMAT,
		tris[i].id,m+1,m+2,m+3);
	if (ncustom) write_custom_all(i);
	fprintf(fp,"\n");
	m += 3;
      }

  }

  fclose(fp);
}

/* ----------------------------------------------------------------------
   write surf file to single or multiple files with no Points section
   each proc has copy of all surfs
------------------------------------------------------------------------- */

void WriteSurf::write_file_all_nopoints(char *file)

{
  // if multiproc, write base file

  if (multiproc && me == 0) write_base(file);

  // open single surface file or multi file for multiproc

  if (filewriter) open(file);

  // nmine = # of surfs I contribute to this file
  // my fraction of all surfs

  int first = static_cast<int> (1.0*me/nprocs * surf->nlocal);
  int next = static_cast<int> (1.0*(me+1)/nprocs * surf->nlocal);
  int nmine = next - first;

  // write header info

  write_header(nmine);

  if (!filewriter) return;

  // istart/istop = lower/upper bound of surfs written by this proc

  int istart = static_cast<int> (1.0*me/nprocs * surf->nlocal);
  int istop = static_cast<int> (1.0*(me+nclusterprocs)/nprocs * surf->nlocal);

  // lines

  if (dim == 2) {
    Surf::Line *lines = surf->lines;
    fprintf(fp,"\nLines\n\n");

    if (typeflag)
      for (int i = istart; i < istop; i++) {
	fprintf(fp,SURFINT_FORMAT " %d %20.15g %20.15g %20.15g %20.15g",
		lines[i].id,lines[i].type,
		lines[i].p1[0],lines[i].p1[1],
		lines[i].p2[0],lines[i].p2[1]);
	if (ncustom) write_custom_all(i);
	fprintf(fp,"\n");
      }

    else
      for (int i = istart; i < istop; i++) {
	fprintf(fp,SURFINT_FORMAT " %20.15g %20.15g %20.15g %20.15g",
		lines[i].id,
		lines[i].p1[0],lines[i].p1[1],
		lines[i].p2[0],lines[i].p2[1]);
	if (ncustom) write_custom_all(i);
	fprintf(fp,"\n");
      }
  }

  // triangles

  if (dim == 3) {
    Surf::Tri *tris = surf->tris;
    fprintf(fp,"\nTriangles\n\n");

    if (typeflag)
      for (int i = istart; i < istop; i++) {
	fprintf(fp,SURFINT_FORMAT " %d %20.15g %20.15g %20.15g "
		"%20.15g %20.15g %20.15g %20.15g %20.15g %20.15g",
		tris[i].id,tris[i].type,
		tris[i].p1[0],tris[i].p1[1],tris[i].p1[2],
		tris[i].p2[0],tris[i].p2[1],tris[i].p2[2],
		tris[i].p3[0],tris[i].p3[1],tris[i].p3[2]);
	if (ncustom) write_custom_all(i);
	fprintf(fp,"\n");
      }
    else
      for (int i = istart; i < istop; i++) {
	fprintf(fp,SURFINT_FORMAT " %20.15g %20.15g %20.15g "
		"%20.15g %20.15g %20.15g %20.15g %20.15g %20.15g",
		tris[i].id,
		tris[i].p1[0],tris[i].p1[1],tris[i].p1[2],
		tris[i].p2[0],tris[i].p2[1],tris[i].p2[2],
		tris[i].p3[0],tris[i].p3[1],tris[i].p3[2]);
	if (ncustom) write_custom_all(i);
	fprintf(fp,"\n");
      }
  }

  fclose(fp);
}

/* ----------------------------------------------------------------------
   write surf file to single or multiple files with Points section
   surfs are distributed across procs (explicit or implicit)
------------------------------------------------------------------------- */

void WriteSurf::write_file_distributed_points(char *file)
{
  // if multiproc, write base file

  if (multiproc && me == 0) write_base(file);

  // open single surface file or multi file for multiproc

  if (filewriter) open(file);

  // nmine = # of surfs I contribute to this file
  // nown/nlocal depending on explicit/implicit

  int nmine;
  if (surf->implicit) nmine = surf->nlocal;
  else nmine = surf->nown;

  // check for overflow

  bigint ndouble,nchar;

  if (dim == 2) {
    ndouble = (bigint) 2*nmine * 2;
    nchar = (bigint) nmine * sizeof(SurfIDType);
  } else {
    ndouble = (bigint) 3*nmine * 3;
    nchar = (bigint) nmine * sizeof(SurfIDType);
  }

  if (ndouble > MAXSMALLINT || nchar > MAXSMALLINT)
    error->one(FLERR,"Too much distributed data to communicate");

  // write header info

  write_header(nmine);

  // each proc contributes explicit or implicit distributed points
  // nmine = element count, npoint = point count
  // nper = size of ID + type
  // lines_mine/tris_mine = ptr in Surf to elements

  int npoint,nper;
  Surf::Line *lines_mine;
  Surf::Tri *tris_mine;

  if (surf->implicit) {
    if (dim == 2) {
      npoint = 2*nmine;
      lines_mine = surf->lines;
    } else {
      npoint = 3*nmine;
      tris_mine = surf->tris;
    }
  } else {
    if (dim == 2) {
      npoint = 2*nmine;
      lines_mine = surf->mylines;
    } else {
      npoint = 3*nmine;
      tris_mine = surf->mytris;
    }
  }
  nper = sizeof(SurfIDType);

  // max_size_point = largest point buffer needed by any proc

  int max_size_point;
  MPI_Allreduce(&npoint,&max_size_point,1,MPI_INT,MPI_MAX,world);

  // pbuf = local buf for my explicit or implicit points

  double *pbuf;
  memory->create(pbuf,max_size_point*dim,"writesurf:pbuf");

  // pack my points into buf

  int m = 0;

  if (dim == 2) {
    for (int i = 0; i < nmine; i++) {
      pbuf[m++] = lines_mine[i].p1[0];
      pbuf[m++] = lines_mine[i].p1[1];
      pbuf[m++] = lines_mine[i].p2[0];
      pbuf[m++] = lines_mine[i].p2[1];
    }
  } else {
    for (int i = 0; i < nmine; i++) {
      pbuf[m++] = tris_mine[i].p1[0];
      pbuf[m++] = tris_mine[i].p1[1];
      pbuf[m++] = tris_mine[i].p1[2];
      pbuf[m++] = tris_mine[i].p2[0];
      pbuf[m++] = tris_mine[i].p2[1];
      pbuf[m++] = tris_mine[i].p2[2];
      pbuf[m++] = tris_mine[i].p3[0];
      pbuf[m++] = tris_mine[i].p3[1];
      pbuf[m++] = tris_mine[i].p3[2];
    }
  }

  // filewriter = 1 = this proc writes to file
  // ping each proc in my cluster, receive its data, write data to file
  // else wait for ping from fileproc, send my data to fileproc

  int recv_size,ncount,tmp;
  MPI_Request request;
  MPI_Status status;

  if (filewriter) {
    fprintf(fp,"\nPoints\n\n");

    bigint index = 0;
    for (int iproc = 0; iproc < nclusterprocs; iproc++) {
      if (iproc) {
        MPI_Irecv(pbuf,max_size_point*dim,MPI_DOUBLE,me+iproc,0,world,&request);
        MPI_Send(&tmp,0,MPI_INT,me+iproc,0,world);
        MPI_Wait(&request,&status);
        MPI_Get_count(&status,MPI_DOUBLE,&recv_size);
      } else recv_size = npoint*dim;

      ncount = recv_size/dim;
      m = 0;

      if (dim == 2) {
        for (int i = 0; i < ncount; i++) {
          index++;
          fprintf(fp,BIGINT_FORMAT " %20.15g %20.15g\n",
                  index,pbuf[m],pbuf[m+1]);
          m += 2;
	}
      } else {
        for (int i = 0; i < ncount; i++) {
          index++;
          fprintf(fp,BIGINT_FORMAT " %20.15g %20.15g %20.15g\n",
                  index,pbuf[m],pbuf[m+1],pbuf[m+2]);
          m += 3;
        }
      }
    }

  } else {
    MPI_Recv(&tmp,0,MPI_INT,fileproc,0,world,&status);
    MPI_Rsend(pbuf,npoint*dim,MPI_DOUBLE,fileproc,0,world);
  }

  memory->sfree(pbuf);

  // max_size_surf = largest surf buffer needed by any proc

  int max_size_surf;
  MPI_Allreduce(&nmine,&max_size_surf,1,MPI_INT,MPI_MAX,world);

  // sbuf = local buf for my surface IDs and types

  SurfIDType *sbuf = (SurfIDType *)
    memory->smalloc(max_size_surf*nper,"writesurf:sbuf");

  // pack my line/tri IDs/types into sbuf

  // if implicit, renumber ids 1 to N to be compatible with read_surf

  bigint bnsurf = nmine;
  bigint offset;
  if (surf->implicit) {
    MPI_Scan(&bnsurf,&offset,1,MPI_SPARTA_BIGINT,MPI_SUM,world);
    offset -= bnsurf;
  }

  m = 0;
  if (dim == 2) {
    for (int i = 0; i < nmine; i++) {
      surfint id = lines_mine[i].id;
      if (surf->implicit)
        id = static_cast<surfint> (offset + i + 1);
      sbuf[i].id = id;
      sbuf[i].type = lines_mine[i].type;
    }
  } else {
    for (int i = 0; i < nmine; i++) {
      surfint id = tris_mine[i].id;
      if (surf->implicit)
        id = static_cast<surfint> (offset + i + 1);
      sbuf[i].id = id;
      sbuf[i].type = tris_mine[i].type;
    }
  }

  // pack my custom data into cvalues

  double **cvalues = NULL;

  if (ncustom) {
    memory->create(cvalues,nmine,nvalues_custom,"write_surf:cvalues");
    pack_custom(nmine,cvalues);
  }

  // filewriter = 1 = this proc writes to file
  // ping each proc in my cluster, receive its data, write data to file
  // else wait for ping from fileproc, send my data to fileproc

  if (filewriter) {
    if (dim == 2) fprintf(fp,"\nLines\n\n");
    else fprintf(fp,"\nTriangles\n\n");
    bigint m = 0;

    for (int iproc = 0; iproc < nclusterprocs; iproc++) {
      if (iproc) {
        MPI_Irecv(sbuf,max_size_surf*nper,MPI_CHAR,me+iproc,0,world,&request);
        MPI_Send(&tmp,0,MPI_INT,me+iproc,0,world);
        MPI_Wait(&request,&status);
        MPI_Get_count(&status,MPI_CHAR,&recv_size);
        if (ncustom) {
          MPI_Irecv(&cvalues[0][0],max_size_surf*nvalues_custom,MPI_DOUBLE,
                    me+iproc,0,world,&request);
          MPI_Send(&tmp,0,MPI_INT,me+iproc,0,world);
          MPI_Wait(&request,&status);
        }
      } else recv_size = nmine*nper;

      ncount = recv_size/nper;
      if (dim == 2) {
	if (typeflag)
	  for (int i = 0; i < ncount; i++) {
	    fprintf(fp,SURFINT_FORMAT " %d " BIGINT_FORMAT " " BIGINT_FORMAT,
		    sbuf[i].id,sbuf[i].type,m+1,m+2);
            if (ncustom) write_custom_distributed(i,cvalues);
            fprintf(fp,"\n");
	    m += 2;
	  }
	else {
	  for (int i = 0; i < ncount; i++) {
	    fprintf(fp,SURFINT_FORMAT " " BIGINT_FORMAT " " BIGINT_FORMAT,
		    sbuf[i].id,m+1,m+2);
            if (ncustom) write_custom_distributed(i,cvalues);
            fprintf(fp,"\n");
	    m += 2;
	  }
	}
	
      } else {
	if (typeflag)
	  for (int i = 0; i < ncount; i++) {
	    fprintf(fp,SURFINT_FORMAT " %d " BIGINT_FORMAT " " BIGINT_FORMAT " "
		    BIGINT_FORMAT,
		    sbuf[i].id,sbuf[i].type,m+1,m+2,m+3);
            if (ncustom) write_custom_distributed(i,cvalues);
            fprintf(fp,"\n");
	    m += 3;
	  }
        else
	  for (int i = 0; i < ncount; i++) {
	    fprintf(fp,SURFINT_FORMAT " " BIGINT_FORMAT " " BIGINT_FORMAT " "
		    BIGINT_FORMAT,
		    sbuf[i].id,m+1,m+2,m+3);
            if (ncustom) write_custom_distributed(i,cvalues);
            fprintf(fp,"\n");
	    m += 3;
	  }
      }
    }
    fclose(fp);

  } else {
    MPI_Recv(&tmp,0,MPI_INT,fileproc,0,world,&status);
    MPI_Rsend(sbuf,nmine*nper,MPI_CHAR,fileproc,0,world);
    if (ncustom) {
      MPI_Recv(&tmp,0,MPI_INT,fileproc,0,world,&status);
      if (nmine)
        MPI_Rsend(&cvalues[0][0],nmine*nvalues_custom,MPI_DOUBLE,
                  fileproc,0,world);
      else
        MPI_Rsend(NULL,nmine*nvalues_custom,MPI_DOUBLE,
                  fileproc,0,world);
    }
  }

  if (ncustom) memory->destroy(cvalues);
  memory->sfree(sbuf);
}

/* ----------------------------------------------------------------------
   write surf file to single or multiple files with no Points section
   surfs are distributed across procs (explicit or implicit)
------------------------------------------------------------------------- */

void WriteSurf::write_file_distributed_nopoints(char *file)
{
  // if multiproc, write base file

  if (multiproc && me == 0) write_base(file);

  // open single surface file or multi file for multiproc

  if (filewriter) open(file);

  // nmine = # of surfs I contribute to this file
  // nown/nlocal depending on explicit/implicit

  int nmine;
  if (surf->implicit) nmine = surf->nlocal;
  else nmine = surf->nown;

  // check for overflow

  bigint nchar;

  if (dim == 2) nchar = (bigint) nmine * sizeof(Surf::Line);
  else nchar = (bigint) nmine * sizeof(Surf::Tri);

  if (nchar > MAXSMALLINT)
    error->one(FLERR,"Too much distributed data to communicate");

  // write header info

  write_header(nmine);

  // each proc contributes explicit or implicit distributed surfs
  // nmine = element count
  // lines_mine/tris_mine = ptr in Surf to elements

  int nper;
  Surf::Line *lines_mine;
  Surf::Tri *tris_mine;

  if (dim == 2) nper = sizeof(Surf::Line);
  else nper = sizeof(Surf::Tri);

  if (surf->implicit) {
    if (dim == 2) lines_mine = surf->lines;
    else tris_mine = surf->tris;
  } else {
    if (dim == 2) lines_mine = surf->mylines;
    else tris_mine = surf->mytris;
  }

  // max_size = largest buffer needed by any proc

  int max_size;
  MPI_Allreduce(&nmine,&max_size,1,MPI_INT,MPI_MAX,world);

  // lines/tris = local buf for my explicit or implicit elements
  // pack my surfs into buf

  Surf::Line *lines;
  Surf::Tri *tris;
  char *buf;

  if (dim == 2) {
    lines = (Surf::Line *) memory->smalloc(max_size*nper,"writesurf:lines");
    memcpy(lines,lines_mine,nmine*nper);
    buf = (char *) lines;
  } else  {
    tris = (Surf::Tri *) memory->smalloc(max_size*nper,"writesurf:tris");
    memcpy(tris,tris_mine,nmine*nper);
    buf = (char *) tris;
  }

  // if implicit, renumber ids 1 to N to be compatible with read_surf

  bigint bnsurf = nmine;
  bigint offset;
  if (surf->implicit) {
    MPI_Scan(&bnsurf,&offset,1,MPI_SPARTA_BIGINT,MPI_SUM,world);
    offset -= bnsurf;
  }

  // pack my custom data into cvalues

  double **cvalues = NULL;

  if (ncustom) {
    memory->create(cvalues,nmine,nvalues_custom,"write_surf:cvalues");
    pack_custom(nmine,cvalues);
  }

  // filewriter = 1 = this proc writes to file
  // ping each proc in my cluster, receive its data, write data to file
  // else wait for ping from fileproc, send my data to fileproc

  int recv_size,ncount,tmp;
  MPI_Request request;
  MPI_Status status;

  if (filewriter) {
    if (dim == 2) fprintf(fp,"\nLines\n\n");
    else fprintf(fp,"\nTriangles\n\n");

    for (int iproc = 0; iproc < nclusterprocs; iproc++) {
      if (iproc) {
        MPI_Irecv(buf,max_size*nper,MPI_CHAR,me+iproc,0,world,&request);
        MPI_Send(&tmp,0,MPI_INT,me+iproc,0,world);
        MPI_Wait(&request,&status);
        MPI_Get_count(&status,MPI_CHAR,&recv_size);
        if (ncustom) {
          MPI_Irecv(&cvalues[0][0],max_size*nvalues_custom,MPI_DOUBLE,
                    me+iproc,0,world,&request);
          MPI_Send(&tmp,0,MPI_INT,me+iproc,0,world);
          MPI_Wait(&request,&status);
        }
      } else recv_size = nmine*nper;

      ncount = recv_size/nper;
      if (dim == 2) {
        lines = (Surf::Line *) buf;
	
	if (typeflag)
	  for (int i = 0; i < ncount; i++) {
            surfint id = lines[i].id;
            if (surf->implicit)
              id = static_cast<surfint> (offset + i + 1);
	    fprintf(fp,SURFINT_FORMAT " %d %20.15g %20.15g %20.15g %20.15g",
		    id,lines[i].type,
		    lines[i].p1[0],lines[i].p1[1],
		    lines[i].p2[0],lines[i].p2[1]);
            if (ncustom) write_custom_distributed(i,cvalues);
            fprintf(fp,"\n");
	  }
	else
	  for (int i = 0; i < ncount; i++) {
            surfint id = lines[i].id;
            if (surf->implicit)
              id = static_cast<surfint> (offset + i + 1);
	    fprintf(fp,SURFINT_FORMAT " %20.15g %20.15g %20.15g %20.15g",
		    id,
		    lines[i].p1[0],lines[i].p1[1],
		    lines[i].p2[0],lines[i].p2[1]);
            if (ncustom) write_custom_distributed(i,cvalues);
            fprintf(fp,"\n");
	  }

      } else {
        tris = (Surf::Tri *) buf;
	
	if (typeflag)
	  for (int i = 0; i < ncount; i++) {
            surfint id = tris[i].id;
            if (surf->implicit)
              id = static_cast<surfint> (offset + i + 1);
	    fprintf(fp,SURFINT_FORMAT " %d %20.15g %20.15g %20.15g "
		    "%20.15g %20.15g %20.15g %20.15g %20.15g %20.15g",
		    id,tris[i].type,
		    tris[i].p1[0],tris[i].p1[1],tris[i].p1[2],
		    tris[i].p2[0],tris[i].p2[1],tris[i].p2[2],
		    tris[i].p3[0],tris[i].p3[1],tris[i].p3[2]);
            if (ncustom) write_custom_distributed(i,cvalues);
            fprintf(fp,"\n");
	  }
	else
	  for (int i = 0; i < ncount; i++) {
            surfint id = tris[i].id;
            if (surf->implicit)
              id = static_cast<surfint> (offset + i + 1);
	    fprintf(fp,SURFINT_FORMAT " %20.15g %20.15g %20.15g "
		    "%20.15g %20.15g %20.15g %20.15g %20.15g %20.15g",
		    id,
		    tris[i].p1[0],tris[i].p1[1],tris[i].p1[2],
		    tris[i].p2[0],tris[i].p2[1],tris[i].p2[2],
		    tris[i].p3[0],tris[i].p3[1],tris[i].p3[2]);
            if (ncustom) write_custom_distributed(i,cvalues);
            fprintf(fp,"\n");
	  }
      }
    }
    fclose(fp);

  } else {
    MPI_Recv(&tmp,0,MPI_INT,fileproc,0,world,&status);
    MPI_Rsend(buf,nmine*nper,MPI_CHAR,fileproc,0,world);
        if (ncustom) {
      MPI_Recv(&tmp,0,MPI_INT,fileproc,0,world,&status);
      if (nmine)
        MPI_Rsend(&cvalues[0][0],nmine*nvalues_custom,MPI_DOUBLE,
                  fileproc,0,world);
      else
        MPI_Rsend(NULL,nmine*nvalues_custom,MPI_DOUBLE,
                  fileproc,0,world);
    }
  }

  if (ncustom) memory->destroy(cvalues);
  memory->sfree(buf);
}

/* ----------------------------------------------------------------------
   write base file for multiproc output
   only called by proc 0
------------------------------------------------------------------------- */

void WriteSurf::write_base(char *file)
{
  char *hfile = new char[strlen(file) + 16];
  char *ptr = strchr(file,'%');
  *ptr = '\0';
  sprintf(hfile,"%s%s%s",file,"base",ptr+1);
  *ptr = '%';

  fp = fopen(hfile,"w");
  if (fp == NULL) {
    char str[128];
    sprintf(str,"Cannot open surface base file %s",hfile);
    error->one(FLERR,str);
  }

  delete [] hfile;

  fprintf(fp,"# Surface element multiproc file written by SPARTA\n\n");
  fprintf(fp,"%d files\n",multiproc);
  if (dim == 2)
    fprintf(fp,BIGINT_FORMAT " lines\n",surf->nsurf);
  else if (dim == 3)
    fprintf(fp,BIGINT_FORMAT " triangles\n",surf->nsurf);

  fclose(fp);
}

/* ----------------------------------------------------------------------
   open a single file or a single multiproc file
   only called by a filewriter proc
------------------------------------------------------------------------- */

void WriteSurf::open(char *file)
{
  char *onefile = file;

  if (multiproc) {
    onefile = new char[strlen(file) + 16];
    char *ptr = strchr(file,'%');
    *ptr = '\0';
    sprintf(onefile,"%s%d%s",file,icluster,ptr+1);
    *ptr = '%';
  }

  fp = fopen(onefile,"w");
  if (fp == NULL) {
    char str[128];
    sprintf(str,"Cannot open surface file %s",onefile);
    error->one(FLERR,str);
  }

  if (multiproc) delete [] onefile;
}

/* ----------------------------------------------------------------------
   write header info into a single file or multiproc file
   called by all procs
   nmine = surf count contributed by this proc,
           only used for multiproc output
------------------------------------------------------------------------- */

void WriteSurf::write_header(int nmine)
{
  // nfile = # of surfs written to this surface file
  // single file: nfile = surf->nsurf
  // multiproc file = sum of nmine over procs contributing to this file

  bigint nfile = surf->nsurf;

  if (multiproc) {
    int tmp,nother;
    MPI_Status status;
    MPI_Request request;

    nfile = 0;
    if (filewriter) {
      for (int iproc = 0; iproc < nclusterprocs; iproc++) {
        if (iproc) {
          MPI_Irecv(&nother,1,MPI_INT,me+iproc,0,world,&request);
          MPI_Send(&tmp,0,MPI_INT,me+iproc,0,world);
          MPI_Wait(&request,&status);
        } else nother = nmine;
        nfile += nother;
      }
    } else {
      MPI_Recv(&tmp,0,MPI_INT,fileproc,0,world,&status);
      MPI_Rsend(&nmine,1,MPI_INT,fileproc,0,world);
    }
  }

  if (filewriter) {
    if (!multiproc)
      fprintf(fp,"# Surface element file written by SPARTA\n\n");
    else
      fprintf(fp,"# Surface element multiproc file written by SPARTA\n\n");
    if (dim == 2) {
      if (pointflag)
        fprintf(fp,BIGINT_FORMAT " points\n",(bigint) 2*nfile);
      fprintf(fp,BIGINT_FORMAT " lines\n",nfile);
    } else if (dim == 3) {
      if (pointflag)
        fprintf(fp,BIGINT_FORMAT " points\n",(bigint) 3*nfile);
      fprintf(fp,BIGINT_FORMAT " triangles\n",nfile);
    }
  }
}

/* ----------------------------------------------------------------------
   pack cvalues with my owned custom data
---------------------------------------------------------------------- */

void WriteSurf::pack_custom(int nmine, double **cvalues)
{
  int m = 0;

  for (int ic = 0; ic < ncustom; ic++) {
    if (type_custom[ic] == 0) {
      if (size_custom[ic] == 0) {
	int *ivector = surf->eivec[surf->ewhich[index_custom[ic]]];
        for (int i = 0; i < nmine; i++)
          cvalues[i][m] = ubuf(ivector[i]).d;
        m++;
      } else {
	int **iarray = surf->eiarray[surf->ewhich[index_custom[ic]]];
	for (int j = 0; j < size_custom[ic]; j++) {
          for (int i = 0; i < nmine; i++)
            cvalues[i][m] = ubuf(iarray[i][j]).d;
          m++;
        }
      }
    } else {
      if (size_custom[ic] == 0) {
	double *dvector = surf->edvec[surf->ewhich[index_custom[ic]]];
        for (int i = 0; i < nmine; i++)
          cvalues[i][m] = dvector[i];
        m++;
      } else {
	double **darray = surf->edarray[surf->ewhich[index_custom[ic]]];
	for (int j = 0; j < size_custom[ic]; j++) {
          for (int i = 0; i < nmine; i++)
            cvalues[i][m] = darray[i][j];
          m++;
        }
      }
    }
  }
}

/* ----------------------------------------------------------------------
   write user-specified custom values for surf I to output file
   called for non-distributed surfs
---------------------------------------------------------------------- */

void WriteSurf::write_custom_all(int i)
{
  for (int ic = 0; ic < ncustom; ic++) {
    if (type_custom[ic] == 0) {
      if (size_custom[ic] == 0) {
	int *ivector = surf->eivec_local[surf->ewhich[index_custom[ic]]];
	fprintf(fp," %d",ivector[i]);
      } else {
	int **iarray = surf->eiarray_local[surf->ewhich[index_custom[ic]]];
	for (int j = 0; j < size_custom[ic]; j++)
	  fprintf(fp," %d",iarray[i][j]);
      }
    } else {
      if (size_custom[ic] == 0) {
	double *dvector = surf->edvec_local[surf->ewhich[index_custom[ic]]];
	fprintf(fp," %g",dvector[i]);
      } else {
	double **darray = surf->edarray_local[surf->ewhich[index_custom[ic]]];
	for (int j = 0; j < size_custom[ic]; j++)
	  fprintf(fp," %g",darray[i][j]);
      }
    }
  }
}

/* ----------------------------------------------------------------------
   write user-specified custom values for surf I to output file
   called for distributed surfs
---------------------------------------------------------------------- */

void WriteSurf::write_custom_distributed(int i, double **cvalues)
{
  int m = 0;
  for (int ic = 0; ic < ncustom; ic++) {
    if (type_custom[ic] == 0) {
      if (size_custom[ic] == 0) {
	fprintf(fp," %d",(int) ubuf(cvalues[i][m++]).i);
      } else {
	for (int j = 0; j < size_custom[ic]; j++)
	  fprintf(fp," %d",(int) ubuf(cvalues[i][m++]).i);
      }
    } else {
      if (size_custom[ic] == 0) {
	fprintf(fp," %g",cvalues[i][m++]);
      } else {
	for (int j = 0; j < size_custom[ic]; j++)
	  fprintf(fp," %g",cvalues[i][m++]);
      }
    }
  }
}
