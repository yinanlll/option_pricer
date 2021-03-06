/* This version of the pricer computes the values of an American put
and call as a function of its maturity, then outputs the values to
text files */

#include <iostream>
#include <fstream>
#include <cmath>
#include <array>
#include <iomanip>
#include <time.h>
#include "fofspot_pricer.h" // defines Matrix template to avoid seg fault
// with large array dimensions

using namespace std;

double phi(double y);
double bs(float values[]);
float fdm(float values[]);

int main()
{
    clock_t t1, t2;
    t1 = clock();

    ofstream call_prices;
    ofstream put_prices;
    ifstream spots;

    call_prices.open("call_prices.txt");
    if (!call_prices) {
        cerr << "Unable to open call_prices.txt. \n";
        exit(1);
    }

    put_prices.open("put_prices.txt");
    if (!put_prices) {
        cerr << "Unable to open put_prices.txt. \n";
        exit(1);
    }

    spots.open("spots.txt");
    if (!spots) {
        cerr << "Unable to open spots.txt. \n";
        exit(1);
    }

    float spo[100];
    for (int i=0; i<100; i++) {
        spots >> spo[i];
    }

    float price;

    float params[11] = {0, 1, 0, 100, 120, 0.5, 0.2, 0.1, 0.05, 500, 500};

    for (int i=0; i<100; i++) {
        params[3] = spo[i];
        price = fdm(params);
        call_prices << spo[i] << ", " << price << "\n";
    }

    params[2] = 1;

    for (int i=0; i<100; i++) {
        params[3] = spo[i];
        price = fdm(params);
        put_prices << spo[i] << ", " << price << "\n";
    }

    t2 = clock();
    cout << "Runtime: " << ((float)t2 - (float)t1)/CLOCKS_PER_SEC << "s" << endl;
    return 0;
}

// compute option price using Black-Scholes formulas
double bs(float values[])
{
    // give the variables names that are easier to work with
    int CP = values[2];
    double S = (double)values[3];
    double K = (double)values[4];
    double tau = (double)values[5];
    double sigma = (double)values[6];
    double r = (double)values[7];
    double a = (double)values[8];

    // declare the constants used in the B-S formula
    double d1 = (1/(sigma*sqrt(tau)))*(log(S/K) + (r-a+(pow(sigma,2)/2))*tau);
    double d2 = (1/(sigma*sqrt(tau)))*(log(S/K) + (r-a-(pow(sigma,2)/2))*tau);
    double price;

    if (CP == 0) {
        price = S*exp(-a*tau)*phi(d1) - K*exp(-r*tau)*phi(d2);
    }

    else {
        price = K*exp(-r*tau)*phi(-d2) - S*exp(-a*tau)*phi(-d1);
    }

    return price;
}

// helper functions for fdm
float aj(int j, int M, float sig, float r, float q, float t) {
    return -0.5*((r-q)*(M-j)*t + pow(sig*(M-j), 2)*t);
}

float bj(int j, int M, float sig, float r, float q, float t) {
    return 1 + pow(sig*(M-j),2)*t + r*t;
}

float cj(int j, int M, float sig, float r, float q, float t) {
    return 0.5*((r-q)*(M-j)*t - pow(sig*(M-j),2)*t);
}

// finite difference method
float fdm(float values[])
{
    // be careful when using M and N to make sure that floats are returned
    // when appropriate
    float EA = values[1];
    float CP = values[2];
    float S = values[3];
    float K = values[4];
    float T = values[5];
    float sig = values[6];
    float r = values[7];
    float q = values[8];
    int M = values[9];
    int N = values[10];

    // mult will be used to determine the maximum stock price of the grid
    int mult = 5;

    // change M slightly to make it easier to recover option price at the end
    M -= (M % mult);
    float Smax = mult*S;

    float s = Smax/M;
    float t = T/N;

    // initialize the grid with all zeros
    Matrix<float> g(M+1,N+1);
    for (int i=0; i<M+1; i++) {
        for (int j=0; j<N+1; j++) {
            g(i,j) = 0;
        }
    }

    // set the boundary conditions corresponding to whether the option
    // is a call or put
    for (int i=0; i<N+1; i++) {
        g(0,i) += (1-CP)*(Smax - K*exp(-(r-q)*(T-t*i)));
        g(M,i) += CP*K*exp(-(r-q)*(T-t*i));
    }

    for (int i=1; i<M; i++) {
        g(i,N) = (1-CP)*std::max((M-i)*s-K, (float)0.0) + CP*std::max(K-(M-i)*s, (float)0.0);
    }

    // make sure the row corresponding to the current price actually
    // contains the current price in the first column and the appropriate
    // intrinsic value in the last column
    g(M-M/mult,0) = S;
    g(M-M/mult,N) = (1-CP)*std::max(S-K, (float)0.0) + CP*std::max(K-S, (float)0.0);

    /* Here we update the grid column-by-column, moving right to left. We have
    to solve a system of linear equations for each column, but we can perform
    back substitution on the coefficient matrices to obtain the solutions,
    which is much faster than inverting them. Think of A below as follows:
    A = [B, b], where B is the square coefficient matrix and b is the vector
    of right-hand side values. We are solving to obtain x = B^{-1}b. */

    Matrix<float> A(M+1,M+1);
    float d[M+1];

    for (int i=N; i>0; i--) {

        // set A equal to zeros, d equal to RHS values from g
        for (int j=0; j<M+1; j++) {
            for (int k=0; k<M+1; k++) {
                A(j,k) = 0;
            }
        }

        // after the first iteration this is inefficient, because d only
        // differs from g(:,i) at its first and last entries
        for (int j=0; j<M+1; j++) {
            d[j] = g(j,i);
        }

        // populate A with the appropriate coefficients
        for (int j=1; j<M; j++) {
            A(j,j-1) = aj(j, M, sig, r, q, t);
            A(j,j) = bj(j, M, sig, r, q, t);
            A(j,j+1) = cj(j, M, sig, r, q, t);
        }

        // implement fast solution of equation Ax=d using special,
        // sparse structure of A
        double a;

        // zero out the first column, which has only one other non-zero entry,
        // and update RHS accordingly
        d[1] -= A(1,0)*d[0];
        A(1,0) = 0;

        // normalize i,i-th entry, zero out everything below it, update RHS
        // we don't need to zero things out, because only the changes in b
        // matter; we end up zeroing out A completely at the beginning of
        // each iteration
        for (int j=1; j<M; j++) {
            a = 1/A(j,j);
            A(j,j) *= a;
            A(j,j+1) *= a;
            d[j] *= a;

            A(j+1,j+1) -= A(j+1,j)*A(j,j+1);
            d[j+1] -= A(j+1,j)*d[j];

            A(j+1,j) = 0;
        }

        // start process over, this time zeroing out the entries above the diagonal
        d[M-1] -= A(M-1,M)*d[M];
        A(M-1,M) = 0;

        // to obtain the correct b we don't need to set A(i-1,i)=0, but since
        // we are reusing A we might as well reset it to the identity
        for (int j=M-1; j>0; j--) {
            d[j-1] -= A(j-1,j)*d[j];
            A(j-1,j) = 0;
        }

        for (int j=0; j<M+1; j++) {
            d[j] = (1-CP)*std::max(d[j], EA*((M-j)*s - K)) + CP*(std::max(d[j], EA*(K-(M-j)*s)));
        }

        for (int j=1; j<M; j++) {
            g(j,i-1) = d[j];
        }
    }

    return g(M - M/mult, 0);
}

// copied this directly from John Cook at https://www.johndcook.com/blog/cpp_phi/
double phi(double x)
{
    // constants
    double a1 =  0.254829592;
    double a2 = -0.284496736;
    double a3 =  1.421413741;
    double a4 = -1.453152027;
    double a5 =  1.061405429;
    double p  =  0.3275911;

    // Save the sign of x
    int sign = 1;
    if (x < 0)
        sign = -1;
    x = fabs(x)/sqrt(2.0);

    // A&S formula 7.1.26
    double t = 1.0/(1.0 + p*x);
    double y = 1.0 - (((((a5*t + a4)*t) + a3)*t + a2)*t + a1)*t*exp(-x*x);

    return 0.5*(1.0 + sign*y);
}
