#include <iostream>

#include "hea/version.h"

int main()
{
    // Ekrana yazma işi yalnızca uygulama katmanında yapılır, core'da değil.
    std::cout << "Akilli Hibrit Enerji Asistani (core " << hea::coreVersion() << ")\n";
    return 0;
}
