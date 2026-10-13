#pragma once

namespace hea {

// Araç gövdesinin (şasi) sabit parametreleri. Hepsi SI biriminde.
// Varsayılan değerler kompakt bir tam hibrit binek araca (Prius sınıfı) yakın seçildi.
// Güç aktarma organı (motor, batarya) parametreleri 3. adımda ayrı bir yapıda gelecek.
struct VehicleParams {
    // Boş araç kütlesi [kg] (sürücü ve yük hariç)
    double curbMass_kg = 1380.0;
    // Aerodinamik direnç katsayısı Cd [-]
    double dragCoefficient = 0.24;
    // Ön kesit alanı A [m^2]
    double frontalArea_m2 = 2.2;
    // Yuvarlanma direnç katsayısı Crr [-] (asfalt üzerinde düşük dirençli lastik)
    double rollingResistanceCoeff = 0.009;
    // Dinamik tekerlek yarıçapı [m]
    double wheelRadius_m = 0.30;
    // Dönen kütle katsayısı delta [-]: tekerlek, aks, motor rotoru gibi dönen
    // parçaların ataletini hesaba katmak için ivme kuvvetinde kütle bu katsayıyla
    // büyütülür (F = delta * m * a). Binek araçlarda tipik olarak 1.03-1.10.
    double rotationalMassFactor = 1.05;

    // Parametreler fiziksel olarak anlamlı mı? (hepsi pozitif, delta >= 1)
    bool isValid() const;
};

} // namespace hea
