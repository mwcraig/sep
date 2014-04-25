/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
* This file is part of SEP
*
* Copyright 2014 Kyle Barbary
*
* SEP is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.

* SEP is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.

* You should have received a copy of the GNU General Public License
* along with SEP.  If not, see <http://www.gnu.org/licenses/>.
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
*
* This file part of: SExtractor
*
* Copyright:         (C) 1993-2011 Emmanuel Bertin -- IAP/CNRS/UPMC
*
* License:           GNU General Public License
*
* SExtractor is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
* SExtractor is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with SExtractor. If not, see <http://www.gnu.org/licenses/>.
*
*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sep.h"
#include "extract.h"

/********************************** cleanprep ********************************/
/*
 * Prepare object for cleaning, by calculating mthresh.
 * This used to be in analyse() / examineiso().
 */

int analysemthresh(int objnb, objliststruct *objlist, int minarea,
		   PIXTYPE dthresh)
{
  objstruct *obj = objlist->obj+objnb;
  pliststruct *pixel = objlist->plist;
  pliststruct *pixt;
  PIXTYPE tpix;
  float     *heap,*heapt,*heapj,*heapk, swap;
  int       j, k, h, status;

  status = RETURN_OK;
  heap = heapt = heapj = heapk = NULL;
  h = minarea;

  if (obj->fdnpix < minarea)
    {
      obj->mthresh = 0.0;
      return status;
    }

  QMALLOC(heap, float, minarea, status);
  heapt = heap;

  /*-- Find the minareath pixel in decreasing intensity for CLEANing */
  for (pixt=pixel+obj->firstpix; pixt>=pixel; pixt=pixel+PLIST(pixt,nextpix))
    {
      /* amount pixel is above threshold */
      tpix = PLISTPIX(pixt, cdvalue) - (PLISTEXIST(dthresh)?
					PLISTPIX(pixt, dthresh):dthresh);
      if (h>0)
        *(heapt++) = (float)tpix;
      else if (h)
        {
	  if ((float)tpix>*heap)
	    {
	      *heap = (float)tpix;
	      for (j=0; (k=(j+1)<<1)<=minarea; j=k)
		{
		  heapk = heap+k;
		  heapj = heap+j;
		  if (k != minarea && *(heapk-1) > *heapk)
		    {
		      heapk++;
		      k++;
		    }
		  if (*heapj <= *(--heapk))
		    break;
		  swap = *heapk;
		  *heapk = *heapj;
		  *heapj = swap;
		}
	    }
        }
      else
        fqmedian(heap, minarea);
      h--;
    }

  obj->mthresh = *heap;

 exit:
  free(heap);
  return status;
}

/************************* preanalyse **************************************/

void  preanalyse(int no, objliststruct *objlist)
{
  objstruct	*obj = &objlist->obj[no];
  pliststruct	*pixel = objlist->plist, *pixt;
  PIXTYPE	peak, cpeak, val, cval;
  double	rv;
  int		x, y, xmin,xmax, ymin,ymax, fdnpix;
  
  /*-----  initialize stacks and bounds */
  fdnpix = 0;
  rv = 0.0;
  peak = cpeak = -BIG;
  ymin = xmin = 2*MAXPICSIZE;    /* to be really sure!! */
  ymax = xmax = 0;

  /*-----  integrate results */
  for (pixt=pixel+obj->firstpix; pixt>=pixel; pixt=pixel+PLIST(pixt,nextpix))
    {
      x = PLIST(pixt, x);
      y = PLIST(pixt, y);
      val=PLISTPIX(pixt, dvalue);
      if (cpeak < (cval=PLISTPIX(pixt, cdvalue)))
	cpeak = cval;
      if (peak < val)
	peak = val;
      rv += cval;
      if (xmin > x)
	xmin = x;
      if (xmax < x)
	xmax = x;
      if (ymin > y)
	ymin = y;
      if (ymax < y)
	ymax = y;
      fdnpix++;
    }    
  
  obj->fdnpix = (LONG)fdnpix;
  obj->fdflux = (float)rv;
  obj->fdpeak = cpeak;
  obj->dpeak = peak;
  obj->xmin = xmin;
  obj->xmax = xmax;
  obj->ymin = ymin;
  obj->ymax = ymax;

  return;
}

/******************************** analyse *********************************/
/*
  If robust = 1, you must have run previously with robust=0
*/

void  analyse(int no, objliststruct *objlist, int robust)
{
  objstruct	*obj = &objlist->obj[no];
  pliststruct	*pixel = objlist->plist, *pixt;
  PIXTYPE	peak, val, cval;
  double	thresh,thresh2, t1t2,darea,
                mx,my, mx2,my2,mxy, rv, tv,
		xm,ym, xm2,ym2,xym,
		temp,temp2, theta,pmx2,pmy2;
  int		x, y, xmin, ymin, area2, dnpix;

  preanalyse(no, objlist);
  
  dnpix = 0;
  mx = my = tv = 0.0;
  mx2 = my2 = mxy = 0.0;
  thresh = obj->dthresh;
  peak = obj->dpeak;
  rv = obj->fdflux;
  thresh2 = (thresh + peak)/2.0;
  area2 = 0;
  
  xmin = obj->xmin;
  ymin = obj->ymin;

  for (pixt=pixel+obj->firstpix; pixt>=pixel; pixt=pixel+PLIST(pixt,nextpix))
    {
      x = PLIST(pixt,x)-xmin;  /* avoid roundoff errors on big images */
      y = PLIST(pixt,y)-ymin;  /* avoid roundoff errors on big images */
      cval = PLISTPIX(pixt, cdvalue);
      tv += (val = PLISTPIX(pixt, dvalue));
      if (val>thresh)
	dnpix++;
      if (val > thresh2)
	area2++;
      mx += cval * x;
      my += cval * y;
      mx2 += cval * x*x;
      my2 += cval * y*y;
      mxy += cval * x*y;
    }

  /* compute object's properties */
  xm = mx / rv;    /* mean x */
  ym = my / rv;    /* mean y */

  /* In case of blending, use previous barycenters */
  if ((robust) && (obj->flag & OBJ_MERGED))
    {
      double xn, yn;
	  
      xn = obj->mx-xmin;
      yn = obj->my-ymin;
      xm2 = mx2 / rv + xn*xn - 2*xm*xn;
      ym2 = my2 / rv + yn*yn - 2*ym*yn;
      xym = mxy / rv + xn*yn - xm*yn - xn*ym;
      xm = xn;
      ym = yn;
    }
  else
    {
      xm2 = mx2 / rv - xm * xm;	 /* variance of x */
      ym2 = my2 / rv - ym * ym;	 /* variance of y */
      xym = mxy / rv - xm * ym;	 /* covariance */
    }

  /* Handle fully correlated x/y (which cause a singularity...) */
  if ((temp2=xm2*ym2-xym*xym)<0.00694)
    {
      xm2 += 0.0833333;
      ym2 += 0.0833333;
      temp2 = xm2*ym2-xym*xym;
      obj->singuflag = 1;
    }
  else
    obj->singuflag = 0;
  
  if ((fabs(temp=xm2-ym2)) > 0.0)
    theta = atan2(2.0 * xym, temp) / 2.0;
  else
    theta = PI/4.0;

  temp = sqrt(0.25*temp*temp+xym*xym);
  pmy2 = pmx2 = 0.5*(xm2+ym2);
  pmx2+=temp;
  pmy2-=temp;
  
  obj->dnpix = (obj->flag & OBJ_OVERFLOW)? obj->fdnpix:(LONG)dnpix;
  obj->dflux = tv;
  obj->mx = xm+xmin;	/* add back xmin */
  obj->my = ym+ymin;	/* add back ymin */
  obj->mx2 = xm2;
  obj->my2 = ym2;
  obj->mxy = xym;
  obj->a = (float)sqrt(pmx2);
  obj->b = (float)sqrt(pmy2);
  obj->theta = theta*180.0/PI;
  
  obj->cxx = (float)(ym2/temp2);
  obj->cyy = (float)(xm2/temp2);
  obj->cxy = (float)(-2*xym/temp2);
  
  darea = (double)area2 - dnpix;
  t1t2 = thresh/thresh2;
  if (t1t2>0.0 && !plistexist_dthresh)  /* was: prefs.dweight_flag */
    {
      obj->abcor = (darea<0.0?darea:-1.0)/(2*PI*log(t1t2<1.0?t1t2:0.99)
					   *obj->a*obj->b);
      if (obj->abcor>1.0)
	obj->abcor = 1.0;
    }
  else
    {
      obj->abcor = 1.0;
    }

  return;

}

