int blah[4];
int frqa;

void square_wave_cog(void *par)
{
    blah[2] = *(int *)par;
    if (_FRQA != frqa) {
        _FRQA = frqa;
    }
}
