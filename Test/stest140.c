int factorial(int n, int prod)
{
    if (n > 0) {
        return factorial(n-1, prod*n);
    }
    return prod;
}
