typedef struct __fromfile("simplepin.spin") PIN;

PIN mypin;

void msg(PIN *pin)
{
    pin->tx(1);
}

void init()
{
    mypin.pin = 8;
    mypin.tx(0);
}
