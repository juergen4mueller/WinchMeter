
#include <Preferences.h>

Preferences prefs;

void saveCalibration(float value) {
    prefs.begin("waage", false);   // Namespace "waage"
    prefs.putFloat("calib", value);
    prefs.end();
}

float loadCalibration() {
    prefs.begin("waage", true);    // read-only
    float value = prefs.getFloat("calib", 2000.0f);  // 0.0 = Default
    prefs.end();
    return value;
}

