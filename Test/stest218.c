int getval(int x)
{
  if (x < 2)
    return 99;
  else
    return 100;
}

int main()
{
  outa = getval(-1);
  outb = getval(2);
  return 0;
}


