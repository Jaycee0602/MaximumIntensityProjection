#include <stdio.h>
#include "ift.h"

#define AXIS_X 0
#define AXIS_Y 1
#define AXIS_Z 2
#define AXIS_H 3

#define GetXCoord(s,p) (((p) % (((s)->xsize)*((s)->ysize))) % (s)->xsize)
#define GetYCoord(s,p) (((p) % (((s)->xsize)*((s)->ysize))) / (s)->xsize)
#define GetZCoord(s,p) ((p) / (((s)->xsize)*((s)->ysize)))

#define ROUND(x) ((x < 0)?(int)(x-0.5):(int)(x+0.5))
#define GetVoxelIndex(s,v) ((v.x)+(s)->tby[(v.y)])

int sign( int x ){
    if(x >= 0)
        return 1;
    return -1;
}

typedef struct volume
{
    iftMatrix* orthogonal;
    iftMatrix* center;
}iftVolumeFaces;


float MatrixInnerProduct(iftMatrix *A, iftMatrix *B)
{
  float result = 0;

  for (int i = 0; i < A->ncols; i++)
    result += (A->val[i] * B->val[i]);

  return result;
}

iftVoxel GetVoxelCoord(iftImage *img, int p)
{
    iftVoxel u;

    u.x = GetXCoord(img, p);
    u.y = GetYCoord(img, p);
    u.z = GetZCoord(img, p);

    return u;
}

/* this function creas the rotation/translation matrix for the given theta */
iftMatrix *createTransformationMatrix(iftImage *img, int xtheta, int ytheta)
{
    iftMatrix *resMatrix = NULL;

    iftVector v1 = {.x = (float)img->xsize / 2.0, .y = (float)img->ysize / 2.0, .z = (float)img->zsize / 2.0};
    iftMatrix *transMatrix1 = iftTranslationMatrix(v1);

    iftMatrix *xRotMatrix = iftRotationMatrix(IFT_AXIS_X, -xtheta);
    iftMatrix *yRotMatrix = iftRotationMatrix(IFT_AXIS_Y, -ytheta);

    float D = sqrt(img->xsize*img->xsize + img->ysize*img->ysize);
    iftVector v2 = {.x = -(D / 2.0), .y = -(D / 2.0), .z = -(D / 2.0)};
    iftMatrix *transMatrix2 = iftTranslationMatrix(v2);


    resMatrix = iftMultMatricesChain(4, transMatrix1, xRotMatrix,yRotMatrix, transMatrix2);

    return resMatrix;
}

int DDA(iftImage *img, iftVoxel p1, iftVoxel pn)
{
    int n, k;
    //iftVoxel p;
    float px, py;
    float J=0;
    int Dx,Dy;
    float dx=0,dy=0;
    //iftCreateMatrix* J;


    if (p1.x == pn.x && p1.y == pn.y)
        n=1;
    else{
        Dx=pn.x - p1.x;
        Dy=pn.y - p1.y;

        if( abs(Dx) >= abs(Dy) ){
            n = abs(Dx)+1;
            dx = sign(Dx);
            dy = (dx * Dy)/Dx;
        }
        else{
            n = abs(Dy)+1;
            dy = sign(Dy);
            dx = (dy * Dx)/Dy;
        }
    }

    px = p1.x;
    py = p1.y;




    for (k = 1; k < n; k++)
    {
        //J+=  (float)LinearInterpolationValue(img, px, py);
        J+=  iftImgVal2D(img, (int)px, (int)py);
        //J+=  (float)LinearInterpolationValue(img, px, py);

        px = px + dx;
        py = py + dy;
    }

    return (int)J;
}


int isValidPoint(iftImage *img, iftVoxel u)
{
    if ((u.x >= 0) && (u.x < img->xsize) &&
        (u.y >= 0) && (u.y < img->ysize)){
        return 1;
    }
    else{
        return 0;
    }
}


int ComputeIntersection(iftMatrix *Tpo, iftImage *img, iftMatrix *Tn, iftVolumeFaces *vf, int *p1, int *pn)
{

    float lambda[6] = { -1};
    int i, p_aux;
    iftMatrix *Nj = iftCreateMatrix(1, 3);
    iftMatrix *DiffCandP0 = iftCreateMatrix(1, 3);
    float NdotNj = 0, DiffShiftDotNj = 0;
    iftVoxel v;
    *p1 = *pn = -1;

    for (i = 0; i < 6; i++) {
      iftMatrixElem(Nj, 0, 0) = vf[i].orthogonal->val[AXIS_X];
      iftMatrixElem(Nj, 0, 1) = vf[i].orthogonal->val[AXIS_Y];
      iftMatrixElem(Nj, 0, 2) = vf[i].orthogonal->val[AXIS_Z];

      NdotNj = MatrixInnerProduct(Tn,Nj);

      iftMatrixElem(DiffCandP0, 0, 0) = vf[i].center->val[AXIS_X] - Tpo->val[AXIS_X];
      iftMatrixElem(DiffCandP0, 0, 1) = vf[i].center->val[AXIS_Y] - Tpo->val[AXIS_Y];
      iftMatrixElem(DiffCandP0, 0, 2) = vf[i].center->val[AXIS_Z] - Tpo->val[AXIS_Z];

      DiffShiftDotNj = MatrixInnerProduct(DiffCandP0,Nj);

      lambda[i]=(float) DiffShiftDotNj/NdotNj;

      v.x = ROUND(Tpo->val[AXIS_X] + lambda[i] * Tn->val[AXIS_X]);
      v.y = ROUND(Tpo->val[AXIS_Y] + lambda[i] * Tn->val[AXIS_Y]);
      v.z = ROUND(Tpo->val[AXIS_Z] + lambda[i] * Tn->val[AXIS_Z]);

      if (isValidPoint(img, v))
      {
          if (*p1 == -1)
              *p1 = GetVoxelIndex(img, v);
          else if (*p1 != -1)
              if (*p1 != GetVoxelIndex(img, v))
                  *pn = GetVoxelIndex(img, v);
      }

    }

    // swap is p1 is greater than pn
    if (*p1 > *pn)
    {
        p_aux = *p1;
        *p1 = *pn;
        *pn = p_aux;
    }

    iftDestroyMatrix(&Nj);
    iftDestroyMatrix(&DiffCandP0);
    if ((*p1 != -1) && (*pn != -1))
        return 1;
    else
        return 0;
}



int LinearInterpolationValue(iftImage *img, iftVoxel v)
{
    //int p[8], i, value;

    return 0;
}

iftVolumeFaces* createVF(iftImage* I)
{
  int Nx = I->xsize;
  int Ny = I->ysize;
  int Nz = I->zsize;
  int i;

  iftVolumeFaces *vf = (iftVolumeFaces *) malloc(sizeof(iftVolumeFaces) * 6);

  for (i = 0; i < 6; i++)
  {
      vf[i].orthogonal = iftCreateMatrix(1, 4);
      vf[i].center = iftCreateMatrix(1, 4);
  }

  // Face of Plane XY
  vf[0].orthogonal->val[AXIS_X] = 0;
  vf[0].orthogonal->val[AXIS_Y] = 0;
  vf[0].orthogonal->val[AXIS_Z] = -1;
  vf[0].orthogonal->val[AXIS_H] = 1;

  vf[0].center->val[AXIS_X] = Nx / 2;
  vf[0].center->val[AXIS_Y] = Ny / 2;
  vf[0].center->val[AXIS_Z] = 0;
  vf[0].center->val[AXIS_H] = 1;

  // Face of Plane XZ
  vf[1].orthogonal->val[AXIS_X] = 0;
  vf[1].orthogonal->val[AXIS_Y] = -1;
  vf[1].orthogonal->val[AXIS_Z] = 0;
  vf[1].orthogonal->val[AXIS_H] = 1;

  vf[1].center->val[AXIS_X] = Nx / 2;
  vf[1].center->val[AXIS_Y] = 0;
  vf[1].center->val[AXIS_Z] = Nz / 2;
  vf[1].center->val[AXIS_H] = 1;

  // Face of Plane YZ
  vf[2].orthogonal->val[AXIS_X] = -1;
  vf[2].orthogonal->val[AXIS_Y] = 0;
  vf[2].orthogonal->val[AXIS_Z] = 0;
  vf[2].orthogonal->val[AXIS_H] = 1;

  vf[2].center->val[AXIS_X] = 0;
  vf[2].center->val[AXIS_Y] = Ny / 2;
  vf[2].center->val[AXIS_Z] = Nz / 2;
  vf[2].center->val[AXIS_H] = 1;

  // Face of Opposite Plane XY
  vf[3].orthogonal->val[AXIS_X] = 0;
  vf[3].orthogonal->val[AXIS_Y] = 0;
  vf[3].orthogonal->val[AXIS_Z] = 1;
  vf[3].orthogonal->val[AXIS_H] = 1;

  vf[3].center->val[AXIS_X] = Nx / 2;
  vf[3].center->val[AXIS_Y] = Ny / 2;
  vf[3].center->val[AXIS_Z] = Nz - 1;
  vf[3].center->val[AXIS_H] = 1;

  // Face of Opposite Plane XZ
  vf[4].orthogonal->val[AXIS_X] = 0;
  vf[4].orthogonal->val[AXIS_Y] = 1;
  vf[4].orthogonal->val[AXIS_Z] = 0;
  vf[4].orthogonal->val[AXIS_H] = 1;

  vf[4].center->val[AXIS_X] = Nx / 2;
  vf[4].center->val[AXIS_Y] = Ny - 1;
  vf[4].center->val[AXIS_Z] = Nz / 2;
  vf[4].center->val[AXIS_H] = 1;

  // Face of Opposite Plane YZ
  vf[5].orthogonal->val[AXIS_X] = 1;
  vf[5].orthogonal->val[AXIS_Y] = 0;
  vf[5].orthogonal->val[AXIS_Z] = 0;
  vf[5].orthogonal->val[AXIS_H] = 1;

  vf[5].center->val[AXIS_X] = Nx - 1;
  vf[5].center->val[AXIS_Y] = Ny / 2;
  vf[5].center->val[AXIS_Z] = Nz / 2;
  vf[5].center->val[AXIS_H] = 1;

  return vf;
}


iftMatrix *voxelToMatrix(iftVoxel v)
{
    iftMatrix *voxMat = iftCreateMatrix(1, 4);
    iftMatrixElem(voxMat, 0, 0) = v.x;
    iftMatrixElem(voxMat, 0, 1) = v.y;
    iftMatrixElem(voxMat, 0, 2) = v.z;
    iftMatrixElem(voxMat, 0, 3) = 1;

    return voxMat;
}



iftImage *MaximumIntensityProjection(iftImage *img, float xtheta, float ytheta)
{
    int diagonal = 0;
    double maxIntensity;
    int p=0;
    int Nu, Nv;
    int p1, pn;
    float* lineVals;
    iftVoxel p0, v1, vn;
    //
    iftVolumeFaces* volumeFaces;
    iftMatrix *Mt1, *Mt2, *Mrx, *Mry, *Mrz, *Mtemp, *T;
    iftMatrix *Norigin, *Nend, *Tnorigin, *Tnend, *Tn;
    iftMatrix *Tpo;

    diagonal = (int)sqrt((double) (img->xsize * img->xsize) + (img->ysize * img->ysize) + (img->zsize * img->zsize));
    Nu = Nv = diagonal;
    iftImage *output = iftCreateImage(Nu, Nv, 1);
    T  = createTransformationMatrix(img, xtheta, ytheta);
    iftPrintMatrix(T);

    Norigin =  iftCreateMatrix(1, 4);
    iftMatrixElem(Norigin, 0, 0) = 0;
    iftMatrixElem(Norigin, 0, 1) = 0;
    iftMatrixElem(Norigin, 0, 2) = 0;
    iftMatrixElem(Norigin, 0, 3) = 1;
    iftPrintMatrix(Norigin);

    Tnorigin = iftMultMatrices(T, Norigin);



    volumeFaces = createVF(img);

    for (p = 0; p < output->n; p++)
    {
        printf("%d\n",p);
        p1 = pn = -1;
        p0 = GetVoxelCoord(output, p);
        Mtemp = voxelToMatrix(p0);
        Tpo = iftMultMatrices(T, Mtemp);

        if (ComputeIntersection(Tpo, img, Tnorigin, volumeFaces, &p1, &pn))
        {
            v1 = GetVoxelCoord(img, p1);
            vn = GetVoxelCoord(img, pn);

            //maxIntensity = IntensityProfile(img, v1, vn);
            maxIntensity=0;

            output->val[p] = maxIntensity;
        }
        else
            output->val[p] = 0;

        iftDestroyMatrix(&Mtemp);
        iftDestroyMatrix(&Tpo);
    }

    iftDestroyMatrix(&T);
    iftDestroyMatrix(&Norigin);
    iftDestroyMatrix(&Tnorigin);
    //iftDestroyMatrix(&Tn);
    //DestroyVolumeFaces(volumeFaces);

    return output;
}


int main(int argc, char *argv[])
{
    //if (argc != 6)
    //    Error("Run: ./main <filename> <output> <xtheta> <ytheta> <ztheta>", "main");

    char buffer[512];

    float tx, ty;
    tx = atof(argv[3]);
    ty = atof(argv[4]);
    //tz = atof(argv[5]);
    char *imgFileName = iftCopyString(argv[1]);
    iftImage *img = iftReadImageByExt(imgFileName);
    iftImage *output = NULL;

    output = MaximumIntensityProjection(img, tx, ty);
    //sprintf(buffer, "data/%.1f%.1f%.1f%s", tx, ty, tz, argv[2]);

    //WriteImageP2(output, buffer);
    //iftDestroyImage(img);
    //iftDestroyImage(output);
    return 0;
}
