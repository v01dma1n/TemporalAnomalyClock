#include "temporal_anomaly_access_point_manager.h"
#include "temporal_anomaly_types.h" 
#include "temporal_anomaly_preferences.h" 
#include "openweather_client.h"

static const PrefSelectOption tempUnitOptions[] = {
    {"Fahrenheit", OWM_UNIT_IMPERIAL},
    {"Celsius", OWM_UNIT_METRIC}
};

// Options for the Anomaly Level (0 to 11)
static const PrefSelectOption anomalyLevelOptions[] = {
    {"0 - Accurate", "0"}, {"1", "1"}, {"2", "2"}, {"3", "3"}, 
    {"4", "4"}, {"5", "5"}, {"6", "6"}, {"7", "7"}, 
    {"8", "8"}, {"9", "9"}, {"10", "10"}, {"11 - Random", "11"}
};
static const int numAnomalyLevels = sizeof(anomalyLevelOptions) / sizeof(PrefSelectOption);

static const int numTempUnitOptions = sizeof(tempUnitOptions) / sizeof(PrefSelectOption);

void TemporalAnomalyAccessPointManager::initializeFormFields() {
    BaseAccessPointManager::initializeFormFields();
    TemporalAnomalyPreferences& appPrefs = static_cast<TemporalAnomalyPreferences&>(_prefs);

    // --- TEMPORAL ANOMALY FIELD ---
    FormField anomalyField;
    anomalyField.id = "anomalyLevel";
    anomalyField.name = "Temporal Anomaly (0-11)";
    anomalyField.isMasked = false;
    anomalyField.prefType = PREF_ENUM; // Use PREF_ENUM for an integer input
    anomalyField.pref.int_pref = reinterpret_cast<int32_t*>(&appPrefs.config.anomalyLevel);
    // Note: To use a SELECT for an enum-style integer, we need a select_options array, 
    // but since we're using PREF_ENUM, the input will default to a number field.
    // We can manually set the options to make it a dropdown.
    anomalyField.select_options = anomalyLevelOptions;
    anomalyField.num_select_options = numAnomalyLevels;
    _formFields.push_back(anomalyField);

    FormField startupAnimField;
    startupAnimField.id = "showStartupAnim";
    startupAnimField.name = "Show Startup Anim";
    startupAnimField.isMasked = false;
    startupAnimField.prefType = PREF_BOOL;
    startupAnimField.pref.bool_pref = &appPrefs.config.showStartupAnimation;
    _formFields.push_back(startupAnimField);

    FormField owmCityField;
    owmCityField.id = "owmCity";
    owmCityField.name = "OWM City";
    owmCityField.isMasked = false;
    owmCityField.prefType = PREF_STRING;
    owmCityField.pref.str_pref = appPrefs.config.owm_city;
    _formFields.push_back(owmCityField);
    
    FormField owmStateField;
    owmStateField.id = "owmState";
    owmStateField.name = "OWM State Code";
    owmStateField.isMasked = false;
    owmStateField.prefType = PREF_STRING;
    owmStateField.pref.str_pref = appPrefs.config.owm_state_code;
    _formFields.push_back(owmStateField);

    FormField owmCountryField;
    owmCountryField.id = "owmCountry";
    owmCountryField.name = "OWM Country Code";
    owmCountryField.isMasked = false;
    owmCountryField.prefType = PREF_STRING;
    owmCountryField.pref.str_pref = appPrefs.config.owm_country_code;
    _formFields.push_back(owmCountryField);

    FormField owmApiKeyField;
    owmApiKeyField.id = "owmApiKey";
    owmApiKeyField.name = "OWM API Key";
    owmApiKeyField.isMasked = true;
    owmApiKeyField.prefType = PREF_STRING;
    owmApiKeyField.pref.str_pref = appPrefs.config.owm_api_key;
    _formFields.push_back(owmApiKeyField);

    FormField tempUnitField;
    tempUnitField.id = "tempUnit";
    tempUnitField.name = "Temp Unit";
    tempUnitField.isMasked = false;
    tempUnitField.prefType = PREF_SELECT;
    tempUnitField.pref.str_pref = appPrefs.config.tempUnit;
    tempUnitField.select_options = tempUnitOptions;
    tempUnitField.num_select_options = numTempUnitOptions;
    _formFields.push_back(tempUnitField);
}
