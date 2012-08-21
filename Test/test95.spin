{ indicate to spin2cpp that it should not output Spin methods }
{++!nospin}

{ the Spin version of the methods }
PUB Square(x)
  return x*x

{++
//
// and here are the C versions of the methods
//
int32_t test95::Square(int32_t x)
{
  return x*x;
}

}
