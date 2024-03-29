// Cpp-for-Conv.cpp
// copyright Horace W. Tso
// Sep 27, 2014

#include <Rcpp.h>
#include <cmath>
#include <algorithm>

using namespace Rcpp;

// [[Rcpp::export]]
NumericVector cpp_normize_against(NumericVector x, NumericVector ctr, NumericVector stds) {
  NumericVector vv(x.size());
  NumericVector vv2(x.size());
  // normalization with ctr / std
  std::transform(x.begin(), x.end(), ctr.begin(), vv.begin(), std::minus<double>());
  std::transform(vv.begin(), vv.end(), stds.begin(), vv2.begin(), std::divides<double>());
  return vv2;
}

// [[Rcpp::export]]
NumericVector cpp_whiten_against(NumericVector x, NumericMatrix U, NumericVector ev) {
  NumericVector vv(U.nrow());
  //v.wh = (t(U) %*% v.norm) / ev
  for ( long i=0; i < U.nrow(); i++) {
    vv(i) = std::inner_product(U(i,_).begin(), U(i,_).end(), x.begin(), 0.0) / ev(i);
  }
  return vv;
}

// [[Rcpp::export]]
NumericVector cpp_triangle_kmeans(NumericVector patch, NumericMatrix cent) {
   NumericVector vv(cent.ncol());
   NumericVector zk(cent.nrow());
   // R: for ( i in 1:n ) z.k[i] = sqrt(sum((cent[i,] - patch)*(cent[i,] - patch)))
   for (long i=0; i < cent.nrow(); i++) {
      NumericMatrix::Row row1 = cent.row(i);
      std::transform( row1.begin(), row1.end(), patch.begin(), vv.begin(), std::minus<double>());
      double res = std::inner_product( vv.begin(), vv.end(), vv.begin(), 0.0);
      zk(i) = std::sqrt(res);
   }
   // R: mu.z = mean(z.k)
   double sum2 = std::accumulate(zk.begin(), zk.end(), 0.0); // must say "0.0" to indicate double, can't just "0"
   double mu_z = sum2 / (double)zk.size();
   // R: z1 = - z.k + mu.z
   for (long i=0; i < zk.size(); i++) {
     zk(i) = - zk(i) + mu_z;
     // pmax(0,z1)
     if ( zk(i) < 0.0 ) {
       zk(i) = 0.0;
     }
   }
   return zk;
}


// [[Rcpp::export]]
IntegerVector cpp_hard_kmeans(NumericVector patch, NumericMatrix cent) {
   NumericVector vv(cent.ncol());
   IntegerVector zi(cent.nrow(), 0);
   double vmin = 100000000.0;
   double res = 0.0;
   long ix = 0;
   
   // R: for ( i in 1:n ) z.k[i] = sqrt(sum((cent[i,] - patch)*(cent[i,] - patch)))
   for (long i=0; i < cent.nrow(); i++) {
      NumericMatrix::Row row1 = cent.row(i);
      std::transform( row1.begin(), row1.end(), patch.begin(), vv.begin(), std::minus<double>());
      res = std::sqrt(std::inner_product( vv.begin(), vv.end(), vv.begin(), 0.0));
      if ( res < vmin ) {
          vmin = res;
          ix = i;
      }
   }
   //int ix = std::min_element(zk.begin(), zk.end()) - zk.begin();
   zi(ix) = 1;
   return zi;
}

// [[Rcpp::export]]
int cpp_hard_kmeans1(NumericVector patch, NumericMatrix cent) {
   NumericVector vv(cent.ncol());
   double vmin = 100000000.0;
   double res = 0.0;
   long ix = 0;

   for (long i=0; i < cent.nrow(); i++) {
      NumericMatrix::Row row1 = cent.row(i);
      std::transform( row1.begin(), row1.end(), patch.begin(), vv.begin(), std::minus<double>());
      res = std::sqrt(std::inner_product( vv.begin(), vv.end(), vv.begin(), 0.0));
      if ( res < vmin ) {
          vmin = res;
          ix = i;
      }
   }
   return ix+1; // C is 0-base, while R is 1-base
}




// [[Rcpp::export]]
NumericVector cpp_subpatch( NumericMatrix m, long rowstart, long colstart,long w ) {
  // this function collapses the matrix by stacking up the rows
  NumericVector vv(w*w);
  long k=0;
  // To stack by rows, the outer loop is over the columns, inner loop over rows 
  for ( long i=0; i < w; i++) { // column
    for ( long j=0; j < w; j++ ) {  // row
      vv(k) = m(rowstart+i, colstart+j);  
      k++;
    }
  }
  return vv;
}

// [[Rcpp::export]]
// TO DO : allow stride > 1 and add padding
NumericMatrix cpp_Conv_Fea(NumericMatrix dat, long w, long stride,
      NumericVector ctr, NumericVector stds, 
      NumericMatrix U, NumericVector ev, 
      NumericMatrix KM,
      int hard) {
  
  long K = KM.nrow();
  long NN = dat.nrow();
  long N = std::sqrt(dat.ncol()/3);
  NumericMatrix Fea(NN, 4*K);
  NumericVector x(dat.ncol());
  
  NumericMatrix rmat(N,N);
  NumericMatrix gmat(N,N);
  NumericMatrix bmat(N,N);
  long Rend=0, Gend=0;
  // the image vector x is arranged by row, ie. rows are consecutive
  // Ex.  x = [ red row 1 | row 2 | ... | green row 1 | row 2 ...| blue row 1 | row 2 | .....]
  
  // First build the matrix structure for each color channel :
  // each row of dat is based on row-major convention, thus the following loop
  
  // temporary variables
  NumericVector vv(w*w*3, 0.0);
  NumericVector vv2(w*w*3, 0.0);
  NumericVector vv3(w*w*3, 0.0);
  NumericVector vv4(w*w*3, 0.0);
  NumericVector vv5(w*w*3, 0.0);
  // placeholders to receive the return from cpp_subpatch()
  NumericVector rv(w*w, 0.0);
  NumericVector gv(w*w, 0.0);
  NumericVector bv(w*w, 0.0);
  NumericVector Pool(4*K, 0.0);
  NumericVector PoolI(K, 0.0);
  NumericVector PoolII(K, 0.0);
  NumericVector PoolIII(K, 0.0);
  NumericVector PoolIV(K, 0.0);
  NumericMatrix patchFea((N-w+1)*(N-w+1), K);

  long Rmid = (long)((N-w+1) / 2);
  long Cmid = (long)((N-w+1) / 2);
  Rend = N*N;
  Gend = N*N*2;
  
  for (long u=0; u < NN; u++) {
    x = dat(u,_);
    // Red channel
    for ( long i=0; i < N; i++) { // row
      for ( long j=0; j < N; j++ ) { // column
        rmat(i,j) = x(i*N + j);
      }
    }
    // Green channel
    for ( long i=0; i < N; i++) {
      for ( long j=0; j < N; j++ ) {
        gmat(i,j) = x(Rend + i*N + j); 
      }
    }
    // Blue channel
    for ( long i=0; i < N; i++) {
      for ( long j=0; j < N; j++ ) {
        bmat(i,j) = x(Gend + i*N + j); 
      }
    }
    // there is no array in Rcpp, so make a matrix instead
    
    // pick subpatches 
    for ( long i=0; i < (N-w+1); i++) { // row
      for ( long j=0; j < (N-w+1); j++ ) { // column
        rv = cpp_subpatch(rmat, i, j, w); // vector of the subpatch of size w*w
        gv = cpp_subpatch(gmat, i, j, w); // vector of the subpatch of size w*w
        bv = cpp_subpatch(bmat, i, j, w); // vector of the subpatch of size w*w
        std::copy(rv.begin(), rv.end(), vv.begin());
        std::copy(gv.begin(), gv.end(), vv.begin() + w*w);
        std::copy(bv.begin(), bv.end(), vv.begin() + 2*w*w);
        // normalization with ctr / std
        std::transform(vv.begin(), vv.end(), ctr.begin(), vv2.begin(), std::minus<double>());
        std::transform(vv2.begin(), vv2.end(), stds.begin(), vv3.begin(), std::divides<double>());
        // whiten using the eigen matrix and singular values
        vv4 = cpp_whiten_against(vv3, U, ev);
        // doesn't matter whether is i*n+j or j*n+i, as long as i use it consistently below
		if ( hard == 0 )
        	vv5 = cpp_triangle_kmeans(vv4, KM);
		else
        	vv5 = cpp_hard_kmeans(vv4, KM);
        patchFea(i*(N-w+1)+j, _) = vv5;
      }
    }
    std::fill(PoolI.begin(),   PoolI.end(), 0.0);
    std::fill(PoolII.begin(),  PoolII.end(), 0.0);
    std::fill(PoolIII.begin(), PoolIII.end(), 0.0);
    std::fill(PoolIV.begin(),  PoolIV.end(), 0.0);
    std::fill(Pool.begin(),    Pool.end(), 0.0);
    
    for (long i=0; i < Rmid; i++) {
      for (long j=0; j < Cmid; j++) {
        PoolI += patchFea(i*(N-w+1)+j, _);     
      }
    }
    for ( long i=0; i < Rmid; i++ ) {
      for ( long j=Cmid; j < (N-w+1); j++ ) {
        PoolII += patchFea(i*(N-w+1)+j, _);
      }
    }
    for ( long i=Rmid; i < (N-w+1); i++ ) {
      for ( long j=Cmid; j < (N-w+1); j++ ) {
        PoolIII += patchFea(i*(N-w+1)+j, _);
      }
    }
    for ( long i=Rmid; i < (N-w+1); i++ ) {
      for ( long j=0; j < Cmid; j++ ) {
        PoolIV += patchFea(i*(N-w+1)+j, _);
      }
    }
    std::copy(PoolI.begin(), PoolI.end(), Pool.begin());
    std::copy(PoolII.begin(), PoolII.end(), Pool.begin() + K);
    std::copy(PoolIII.begin(), PoolIII.end(), Pool.begin() + 2*K);
    std::copy(PoolIV.begin(), PoolIV.end(), Pool.begin() + 3*K);
    
    Fea(u,_) = Pool;
  }
  return Fea;
}


// [[Rcpp::export]]
NumericMatrix cpp_list2mat(List Chan, long N, long w, NumericVector ctr, NumericVector stds) {
  
  NumericVector vv(w*w*3);
  NumericVector vv2(w*w*3);
  NumericVector vv3(w*w*3);
  // placeholders to receive the return from cpp_subpatch()
  NumericVector rv(w*w);
  NumericVector gv(w*w);
  NumericVector bv(w*w);
  // there is no array in Rcpp, so make a matrix instead
  NumericMatrix patchK((N-w+1)*(N-w+1), w*w*3);
  // pick subpatches 
  for ( long i=0; i < (N-w+1); i++) {
    for ( long j=0; j < (N-w+1); j++ ) {
      rv = cpp_subpatch(Chan["red"], i, j, w); // vector of the subpatch of size w*w
      gv = cpp_subpatch(Chan["green"], i, j, w); // vector of the subpatch of size w*w
      bv = cpp_subpatch(Chan["blue"], i, j, w); // vector of the subpatch of size w*w
      // concatenating three vectors
      std::copy(rv.begin(), rv.end(), vv.begin());
      std::copy(gv.begin(), gv.end(), vv.begin() + w*w);
      std::copy(bv.begin(), bv.end(), vv.begin() + 2*w*w);
      // normalization with ctr / std
      std::transform(vv.begin(), vv.end(), ctr.begin(), vv2.begin(), std::minus<double>());
      std::transform(vv2.begin(), vv2.end(), stds.begin(), vv3.begin(), std::divides<double>());
      patchK(i*(N-w+1)+j, _) = vv3;
    }
  }
  return patchK;
}  


// [[Rcpp::export]]
List cpp_im2chan(NumericVector x, long N) {

  NumericMatrix rmat(N,N);
  NumericMatrix gmat(N,N);
  NumericMatrix bmat(N,N);
  long Rend=0, Gend=0;
  // the image vector x is arranged by row, ie. rows are consecutive
  // Ex.  x = [ red row1 | row2 | ... | green row 1 | row 2 ...| blue row 1 | row 2 | .....]
  for ( long j=0; j < N; j++) {
    for ( long i=0; i < N; i++ ) {
      rmat(i,j) = x(i*N + j);
    }
  }
  Rend = N*N;
  // Green channel
  for ( long j=0; j < N; j++) {
    for ( long i=0; i < N; i++ ) {
      gmat(i,j) = x(Rend + i*N + j); 
    }
  }
  Gend = N*N*2;
  // Blue channel
  for ( long j=0; j < N; j++) {
    for ( long i=0; i < N; i++ ) {
      bmat(i,j) = x(Gend + i*N + j); 
    }
  }
  return List::create(Named("red", rmat), Named("green", gmat), Named("blue",bmat));
}




// [[Rcpp::export]]
NumericMatrix cpp_v2m(NumericVector x, long nr, long nc ) {
  
  NumericMatrix m1(nr, nc);
  // assume column-major for matrix construction
  for ( long j=0; j < nc; j++) {
    for ( long i=0; i < nr; i++ ) {
      m1(i,j) = x(j*nr+i);  
    }
  }
  return m1;
}

// [[Rcpp::export]]
NumericVector cpp_m2v(NumericMatrix m, long nr, long nc ) {
  NumericVector vv(nr*nc);
  long k=0;
  // assume column-major for matrix construction
  for ( long j=0; j < nc; j++) {
    for ( long i=0; i < nr; i++ ) {
      vv(k) = m(i,j);  
      k++;
    }
  }
  return vv;
}



