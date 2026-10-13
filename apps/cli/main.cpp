#include <cstdio>

#include "hea/units.h"
#include "hea/vehicle_model.h"
#include "hea/version.h"

// Geçici: araç modelinin sabit hızda ürettiği yol yükünü tablo olarak gösterir.
// Simülasyon döngüsü eklendiğinde (5. adım) yerini adım çıktılarına bırakacak.
static void printRoadLoadTable(const hea::VehicleModel& model, double gradePercent)
{
    const double theta = hea::units::gradePercentToRad(gradePercent);
    std::printf("\nSabit hiz yol yuku (egim %%%.0f, yuk 75 kg)\n", gradePercent);
    std::printf("%8s %9s %9s %9s %9s %10s\n",
                "km/h", "aero[N]", "yuv[N]", "egim[N]", "top[N]", "guc[kW]");
    for (int kmh = 0; kmh <= 140; kmh += 20) {
        const double v = hea::units::kmhToMps(kmh);
        const hea::RoadLoad load = model.roadLoad(v, 0.0, theta, 75.0);
        std::printf("%8d %9.1f %9.1f %9.1f %9.1f %10.2f\n", kmh,
                    load.aero_N, load.rolling_N, load.grade_N, load.total_N,
                    hea::VehicleModel::wheelPower(load.total_N, v) / 1000.0);
    }
}

int main()
{
    // Ekrana yazma işi yalnızca uygulama katmanında yapılır, core'da değil.
    std::printf("Akilli Hibrit Enerji Asistani (core %s)\n", hea::coreVersion());

    const hea::VehicleModel model{hea::VehicleParams{}};
    printRoadLoadTable(model, 0.0);
    printRoadLoadTable(model, 5.0);
    return 0;
}
