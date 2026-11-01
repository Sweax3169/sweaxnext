// SWEAX NEXT — Tests/main.cpp
#include "Cati.h"
#include "Decision/Feedback.h"

int main (int argc, char** argv)
{
    // geri besleme turları makine hızından bağımsız: yük altında ya da CI'da aynı plan (Feedback.h)
    sweax::decision::Feedback::zamanSiniriYok = true;
    return sweax::test::hepsiniCalistir (argc, argv);
}
