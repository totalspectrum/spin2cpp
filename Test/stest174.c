void main()
{
     int       Int_1_Loc;

    Int_1_Loc = 1;
    Proc_2(&Int_1_Loc);
    Proc_5(&Int_1_Loc);
}

void Proc_2 (Int_Par_Ref)
int *Int_Par_Ref;
{
    _OUTA = *Int_Par_Ref;
}

void Proc_5(Int_Par_Ref)
int *Int_Par_Ref;
{
    _INA = *Int_Par_Ref;
}

