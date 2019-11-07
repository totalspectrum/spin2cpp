int g;

void setval(int &a, const int b)
{
    a = b;
}

void setg(int v)
{
    setval(g, v);
}
