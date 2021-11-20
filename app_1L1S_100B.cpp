/* ----------------------------------------------------------------------
PFDD -- Phase Field Dislocation Dynamics

Copyright 2021 Los Alamos National Laboratory. See LICENSE.md
   ------------------------------------------------------------------------- */

#include "math.h"
#include "string.h"
#include "stdlib.h"
#include "app_1L1S_100B.h"
#include "fft.h"
#include "solve.h"
#include "random_park.h"
#include "error.h"

using namespace PFDD_NS;

/* ---------------------------------------------------------------------- */

App1L1S100B::App1L1S100B(PFDD_C *pfdd_p, int narg, char **arg) :
  App(pfdd_p,narg,arg)
{
  ndouble = 0;
  timestep = 1.0;
  //neighshell = 1;
  slip_systems = 1;  // The # of slip systems has to be allocated in the constructure
                      //  fot the FFT class to be able to allocate arrays
  num_planes = 1;  // The # of plane normals has to be allocated in the constructure
                      //  fot the FFT class to be able to allocate arrays

  CD = 0.5;           // Dislocation mobility
  oflag = 1;

  if(setup_flag == -1)
    setup_flag = 11; //default to straight nonortho
  if(core_flag == -1)
    core_flag = 1; //default to sin^2
}

/* ---------------------------------------------------------------------- */

App1L1S100B::~App1L1S100B()
{
  delete [] sites;
}

/* ----------------------------------------------------------------------
   set site value ptrs each time iarray/darray are reallocated
   ------------------------------------------------------------------------- */

void App1L1S100B::grow_app()
{
  darray = fft->xi;
}

/* ----------------------------------------------------------------------
   initialize before each run
   check validity of site values
   ------------------------------------------------------------------------- */

void App1L1S100B::init_app()
{
  delete [] sites;
  sites = new int[1 + maxneigh];

  grow_app();
  set_slip();
  initial_sxtal();

  stopsteps = static_cast<int>(floor(stoptime/timestep));
}
/* ----------------------------------------------------------------------
   input specific for this app
   ------------------------------------------------------------------------- */
void App1L1S100B::input_app(char *command, int narg, char **arg)
{
  error->all(FLERR,"Unrecognized command");
}

/* ----------------------------------------------------------------------
   compute energy of site
   ------------------------------------------------------------------------- */

// double App1L1S100B::site_energy(int i)
// {
//   int isite = spin[i];
//   int eng = 0;
//   for (int j = 0; j < numneigh[i]; j++)
//     if (isite != spin[neighbor[i][j]]) eng++;
//   return (double) eng;
// }

/* ----------------------------------------------------------------------
   rKMC method
   perform a site event with null bin rejection
   flip randomly to either spin
   null bin extends to size 2
   ------------------------------------------------------------------------- */

// void App1L1S100B::site_event_rejection(int i, RandomPark *random)
// {
//   int oldstate = spin[i];
//   double einitial = site_energy(i);

//   // event = random spin from 1 to 2, including self

//   if (random->uniform() < 0.5) spin[i] = 1;
//   else spin[i] = 2;
//   double efinal = site_energy(i);

//   // accept or reject via Boltzmann criterion

//   if (efinal <= einitial) {
//   } else if (temperature == 0.0) {
//     spin[i] = oldstate;
//   } else if (random->uniform() > exp((einitial-efinal)*t_inverse)) {
//     spin[i] = oldstate;
//   }

//   if (spin[i] != oldstate) naccept++;

//   // set mask if site could not have changed
//   // if site changed, unset mask of sites with affected propensity
//   // OK to change mask of ghost sites since never used

//   if (Lmask) {
//     if (einitial < 0.5*numneigh[i]) mask[i] = 1;
//     if (spin[i] != oldstate)
//       for (int j = 0; j < numneigh[i]; j++)
// 	mask[neighbor[i][j]] = 0;
//   }
// }

/* ----------------------------------------------------------------------
   KMC method
   compute total propensity of owned site summed over possible events
   ------------------------------------------------------------------------- */

// double App1L1S100B::site_propensity(int i)
// {
//   // event = spin flip

//   int oldstate = spin[i];
//   int newstate = 1;
//   if (oldstate == 1) newstate = 2;

//   // compute energy difference between initial and final state
//   // if downhill or no energy change, propensity = 1
//   // if uphill energy change, propensity = Boltzmann factor

//   double einitial = site_energy(i);
//   spin[i] = newstate;
//   double efinal = site_energy(i);
//   spin[i] = oldstate;

//   if (efinal <= einitial) return 1.0;
//   else if (temperature == 0.0) return 0.0;
//   else return exp((einitial-efinal)*t_inverse);
// }

/* ----------------------------------------------------------------------
   KMC method
   choose and perform an event for site
   ------------------------------------------------------------------------- */

// void App1L1S100B::site_event(int i, RandomPark *random)
// {
//   int m;

//   // perform event = spin flip

//   if (spin[i] == 1) spin[i] = 2;
//   else spin[i] = 1;

//   // compute propensity changes for self and neighbor sites
//   // ignore update of neighbor sites with isite < 0

//   int nsites = 0;
//   int isite = i2site[i];
//   sites[nsites++] = isite;
//   propensity[isite] = site_propensity(i);

//   for (int j = 0; j < numneigh[i]; j++) {
//     m = neighbor[i][j];
//     isite = i2site[m];
//     if (isite < 0) continue;
//     sites[nsites++] = isite;
//     propensity[isite] = site_propensity(m);
//   }

//   solve->update(nsites,sites,propensity);
// }

/* ----------------------------------------------------------------------
   New
   returns the spin of one site
   ------------------------------------------------------------------------- */

// int App1L1S100B::site_spin (int i) {
//   if ((i<0)||(i>nlocal))
//     error->all (FLERR, "Invalid site index");
//   return spin[i];
// }


void App1L1S100B::set_slip()
{
  int i, j, k;
  double s3;
  double xnf=0,ynf=0,znf=0;
  double xbf=0,ybf=0,zbf=0;

  fft->xn[0][0]= 0;
  fft->xn[0][1]= 0;
  fft->xn[0][2]= 1.0;

  fft->xb[0][0]= 1.0;
  fft->xb[0][1]= 0.0;   // screw, HJ
  fft->xb[0][2]= 0.0;

  // Rotation
  // for(int i=0; i<slip_systems; i++){
  //   xnf = (fft->xn[i][0]*fft->four[0][0] + fft->xn[i][1]*fft->four[0][1] + fft->xn[i][2]*fft->four[0][2])
  //     /fft->normfour[0];
  //   ynf = (fft->xn[i][0]*fft->four[1][0] + fft->xn[i][1]*fft->four[1][1] + fft->xn[i][2]*fft->four[1][2])
  //     /fft->normfour[1];
  //   znf = (fft->xn[i][0]*fft->four[2][0] + fft->xn[i][1]*fft->four[2][1] + fft->xn[i][2]*fft->four[2][2])
  //     /fft->normfour[2];

  //   fft->xn[i][0] = xnf;
  //   fft->xn[i][1] = ynf;
  //   fft->xn[i][2] = znf;

  //   xbf = (fft->xb[i][0]*fft->four[0][0] + fft->xb[i][1]*fft->four[0][1] + fft->xb[i][2]*fft->four[0][2])
  //     /fft->normfour[0];
  //   ybf = (fft->xb[i][0]*fft->four[1][0] + fft->xb[i][1]*fft->four[1][1] + fft->xb[i][2]*fft->four[1][2])
  //     /fft->normfour[1];
  //   zbf = (fft->xb[i][0]*fft->four[2][0] + fft->xb[i][1]*fft->four[2][1] + fft->xb[i][2]*fft->four[2][2])
  //     /fft->normfour[2];

  //   fft->xb[i][0] = xbf;
  //   fft->xb[i][1] = ybf;
  //   fft->xb[i][2] = zbf;
  // }
  return;
}

/* ----------------------------------------------------------------------
   computes the xi in a given point and slip system
   ------------------------------------------------------------------------- */

double App1L1S100B::compute_mean_xi(int n, int flag)
{
  int lN1 = fft->local_x;
  int lxs = fft->local_x_start;
  int N2 = fft->local_y;
  int N3 = fft->local_z;
  int NS = fft->slip_systems;
  int slip = 0;
  int na=-1;

  int i=siteijk[n][0] - lxs;
  int j=siteijk[n][1];
  int k=siteijk[n][2];

  if(flag == 1){ // 1 slip system, real value
    slip = 0;
    na = 2*(i*N2*N3 + j*N3 + k + slip*lN1*N2*N3);
  }
  else if(flag == 2){ // 1 slip system, imaginary value
    slip = 0;
    na = 2*(i*N2*N3 + j*N3 + k + slip*lN1*N2*N3) + 1;
  }

  return fft->xi[0][na];
}

/* ----------------------------------------------------------------------
   computes the stress in a given point
   ------------------------------------------------------------------------- */

double App1L1S100B::compute_stress(int n, int flag)
{
  int lN1 = fft->local_x;
  int lxs = fft->local_x_start;
  int N2 = fft->local_y;
  int N3 = fft->local_z;
  int NS = fft->slip_systems;
  int ND = dimension;
  int slip = 0;
  int na=-1;

  int i=siteijk[n][0] - lxs;
  int j=siteijk[n][1];
  int k=siteijk[n][2];

  if(flag == 1)  // pxx
    na = 2*(i*N2*N3 + j*N3 + k + 0*lN1*N2*N3 + 0*lN1*N2*N3*ND);
  else if(flag == 2)  // pyy
    na = 2*(i*N2*N3 + j*N3 + k + 1*lN1*N2*N3 + 1*lN1*N2*N3*ND);
  else if(flag == 3)  // pzz
    na = 2*(i*N2*N3 + j*N3 + k + 2*lN1*N2*N3 + 2*lN1*N2*N3*ND);
  else if(flag == 4)  // pxy
    na = 2*(i*N2*N3 + j*N3 + k + 0*lN1*N2*N3 + 1*lN1*N2*N3*ND);
  else if(flag == 5)  // pxz
    na = 2*(i*N2*N3 + j*N3 + k + 0*lN1*N2*N3 + 2*lN1*N2*N3*ND);
  else if(flag == 6)  // pxz
    na = 2*(i*N2*N3 + j*N3 + k + 1*lN1*N2*N3 + 2*lN1*N2*N3*ND);

  return fft->data_sigma[na];
}

/* ----------------------------------------------------------------------
   computes theta in a given point and slip system
   ------------------------------------------------------------------------- */

double App1L1S100B::compute_theta(int n, int flag)
{
  int lN1 = fft->local_x;
  int lxs = fft->local_x_start;
  int N2 = fft->local_y;
  int N3 = fft->local_z;
  int NS = fft->slip_systems;
  int slip = 0;
  int na=-1;

  int i=siteijk[n][0] - lxs;
  int j=siteijk[n][1];
  int k=siteijk[n][2];

  if(flag == 1){ // 1 slip system, real value
    slip = 0;
    na = (i*N2*N3 + j*N3 + k + slip*lN1*N2*N3);
  }

  return fft->theta[na];
}

double App1L1S100B::compute_delta(int n, int flag)
{
  int lN1 = fft->local_x;
  int lxs = fft->local_x_start;
  int N2 = fft->local_y;
  int N3 = fft->local_z;
  int NS = fft->slip_systems;
  int slip = 0;
  int na=-1;

  int i=siteijk[n][0] - lxs;
  int j=siteijk[n][1];
  int k=siteijk[n][2];

  if(flag == 1){ // 1 slip system
    slip = 0;
    na = (i*N2*N3 + j*N3 + k + slip*lN1*N2*N3);
  }
  return fft->delta[na];
}

double App1L1S100B::compute_ddelta(int n, int flag)
{
  int lN1 = fft->local_x;
  int lxs = fft->local_x_start;
  int N2 = fft->local_y;
  int N3 = fft->local_z;
  int NS = fft->slip_systems;
  int slip = 0;
  int na=-1;

  int i=siteijk[n][0] - lxs;
  int j=siteijk[n][1];
  int k=siteijk[n][2];

  if(flag == 1){ // 1 slip system
    slip = 0;
    na = (i*N2*N3 + j*N3 + k + slip*lN1*N2*N3);
  }

  return fft->ddelta[na];
}

/* ----------------------------------------------------------------------
   computes the strain in a given point
   ------------------------------------------------------------------------- */

double App1L1S100B::compute_strain(int n, int flag)
{
  int lN1 = fft->local_x;
  int lxs = fft->local_x_start;
  int N2 = fft->local_y;
  int N3 = fft->local_z;
  int NS = fft->slip_systems;
  int ND = dimension;
  int slip = 0;
  int na=-1;

  int i=siteijk[n][0] - lxs;
  int j=siteijk[n][1];
  int k=siteijk[n][2];

  if(flag == 1)  // exx
    na = 2*(i*N2*N3 + j*N3 + k + 0*lN1*N2*N3 + 0*lN1*N2*N3*ND);
  else if(flag == 2)  // eyy
    na = 2*(i*N2*N3 + j*N3 + k + 1*lN1*N2*N3 + 1*lN1*N2*N3*ND);
  else if(flag == 3)  // ezz
    na = 2*(i*N2*N3 + j*N3 + k + 2*lN1*N2*N3 + 2*lN1*N2*N3*ND);
  else if(flag == 4)  // exy
    na = 2*(i*N2*N3 + j*N3 + k + 0*lN1*N2*N3 + 1*lN1*N2*N3*ND);
  else if(flag == 5)  // exz
    na = 2*(i*N2*N3 + j*N3 + k + 0*lN1*N2*N3 + 2*lN1*N2*N3*ND);
  else if(flag == 6)  // exz
    na = 2*(i*N2*N3 + j*N3 + k + 1*lN1*N2*N3 + 2*lN1*N2*N3*ND);

  return fft->data_eps[na];
}
/* ----------------------------------------------------------------------
   print stats
------------------------------------------------------------------------- */

void App1L1S100B::stats(char *strtmp)
{
  char *strpnt = strtmp;
  sprintf(strpnt," %10g %10d",time,steps);
  strpnt += strlen(strpnt);

  // for (int m = 0; m < nspecies; m++) {
  //   sprintf(strpnt," %d",pcount[m]);
  //   strpnt += strlen(strpnt);
  // }
}

/* ----------------------------------------------------------------------
   print stats header
------------------------------------------------------------------------- */

void App1L1S100B::stats_header(char *strtmp)
{
  char *strpnt = strtmp;
  sprintf(strpnt," %10s %10s","Time","Step");
  strpnt += strlen(strpnt);

  // for (int m = 0; m < nspecies; m++) {
  //   sprintf(strpnt," %s",sname[m]);
  //   strpnt += strlen(strpnt);
  // }
}
