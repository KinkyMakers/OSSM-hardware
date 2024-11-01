#ifndef OSSM_SOFTWARE_FR_H
#define OSSM_SOFTWARE_FR_H

#include "structs/LanguageStruct.h"

static const String fr_DeepThroatTrainerSync PROGMEM = "DeepThroat Sync";
static const String fr_Error PROGMEM = "Erreur";
static const String fr_GetHelp PROGMEM = "Aide";
static const String fr_GetHelpLine1 PROGMEM = "Sur Discord,";
static const String fr_GetHelpLine2 PROGMEM = "ou GitHub";
static const String fr_Homing PROGMEM = "Calibrage";
static const String fr_HomingTookTooLong PROGMEM =
    "Le calibrage a pris trop de temps. Vérifiez le câblage et réessayez.";
static const String fr_Idle PROGMEM = "Initialisation";
static const String fr_InDevelopment PROGMEM =
    "Cette fonctionnalité est en développement.";
static const String fr_MeasuringStroke PROGMEM = "Mesure de la course";
static const String fr_NoInternalLoop PROGMEM 
    "Aucun gestionnaire d'affichage implémenté.";
static const String fr_Restart PROGMEM = "Redémarrer";
static const String fr_Settings PROGMEM = "Paramètres";
static const String fr_SimplePenetration PROGMEM = "Pénétration simple";
static const String fr_Skip PROGMEM = "Cliquez pour quitter";
static const String fr_Speed PROGMEM = "Vitesse";
static const String fr_SpeedWarning PROGMEM =
    "Réduisez la vitesse pour commencer.";
static const String fr_StateNotImplemented PROGMEM = "État: %u non implémenté.";
static const String fr_Stroke PROGMEM = "Course";
static const String fr_StrokeEngine PROGMEM = "Moteur de course";
static const String fr_StrokeTooShort PROGMEM =
    "Course trop courte. Vérifiez votre courroie d'entraînement.";
static const String fr_Update PROGMEM = "Mise à jour";
static const String fr_UpdateMessage PROGMEM =
    "Mise à jour en cours. Cela peut prendre jusqu'à 60s.";
static const String fr_WiFi PROGMEM = "Wi-Fi";
static const String fr_WiFiSetup PROGMEM = "Configuration Wi-Fi";
static const String fr_WiFiSetupLine1 PROGMEM = "Connectez-vous à";
static const String fr_WiFiSetupLine2 PROGMEM = "'Ossm Setup'";
static const String fr_YouShouldNotBeHere PROGMEM =
    "Vous ne devriez pas être ici.";
static const String fr_AdvancedConfiguration PROGMEM = "Configuration avancée";

static const String fr_StrokeEngineDescriptions[] PROGMEM = {
    "Accélération, glissement, décélération également répartis; sans "
    "sensation.",
    "La vitesse varie avec la sensation; équilibre les courses rapides.",
    "La sensation varie l'accélération; de robotique à progressive.",
    "Alterne courses complètes et mi-profondeur; sensation affecte la vitesse.",
    "La profondeur augmente par cycle; sensation définit le nombre.",
    "Pauses entre les courses; sensation ajuste la durée.",
    "Modifie la longueur, maintient la vitesse; sensation influence la "
    "direction."};

static const String fr_StrokeEngineNames[] PROGMEM = {
    "Course Simple", "Frappe Taquine", "Course Robot", "Demi-Course",
    "Plus Profond",  "Arrêt-Reprise",  "Insistant"};

static const String fr_AdvancedConfigurationSettingNames[] PROGMEM = {
    "Lisez-moi config. av.", "Direction moteur",      "Pas par révolution",
    "Dents poulie",          "Vitesse max",           "Longueur rail",
    "Calibrage rapide",      "Sensibilité calibrage", "Réinit. par défaut",
    "Appliquer param."};

static const String fr_AdvancedConfigurationSettingDescriptions[] PROGMEM = {
    "Appui long pour éditer.\nAppui court pour définir valeur.\nChangements "
    "annulés si non appliqués.",
    "Bascule direction calibrage.\nProfondeur max doit être côté pénétration "
    "loin de la tête OSSM.",
    "Réglez selon votre registre moteur ou réglage des commutateurs DIP.",
    "Spécifiez le nombre de dents sur votre poulie.",
    "Limite de vitesse en millimètres par seconde.",
    "La longueur de votre rail en millimètres.",
    "(Futur) Option pour exécuter une procédure de calibrage plus rapide",
    "Multiplicateur du courant au repos.\nValeur plus élevée surmonte plus de "
    "friction.",
    "Restaurer paramètres par défaut et redémarrer OSSM?\nOui: Appui "
    "long\nNon: Appui court",
    "Sauvegarder changements et redémarrer OSSM?\nOui: Appui long\nNon: Appui "
    "court"};

static const LanguageStruct fr = {
    .DeepThroatTrainerSync = fr_DeepThroatTrainerSync,
    .Error = fr_Error,
    .GetHelp = fr_GetHelp,
    .GetHelpLine1 = fr_GetHelpLine1,
    .GetHelpLine2 = fr_GetHelpLine2,
    .Homing = fr_Homing,
    .HomingTookTooLong = fr_HomingTookTooLong,
    .Idle = fr_Idle,
    .InDevelopment = fr_InDevelopment,
    .MeasuringStroke = fr_MeasuringStroke,
    .NoInternalLoop = fr_NoInternalLoop,
    .Restart = fr_Restart,
    .Settings = fr_Settings,
    .SimplePenetration = fr_SimplePenetration,
    .Skip = fr_Skip,
    .Speed = fr_Speed,
    .SpeedWarning = fr_SpeedWarning,
    .StateNotImplemented = fr_StateNotImplemented,
    .Stroke = fr_Stroke,
    .StrokeEngine = fr_StrokeEngine,
    .StrokeTooShort = fr_StrokeTooShort,
    .Update = fr_Update,
    .UpdateMessage = fr_UpdateMessage,
    .WiFi = fr_WiFi,
    .WiFiSetup = fr_WiFiSetup,
    .WiFiSetupLine1 = fr_WiFiSetupLine1,
    .WiFiSetupLine2 = fr_WiFiSetupLine2,
    .YouShouldNotBeHere = fr_YouShouldNotBeHere,
    .StrokeEngineDescriptions = fr_StrokeEngineDescriptions,
    .StrokeEngineNames = fr_StrokeEngineNames,
    .AdvancedConfiguration = fr_AdvancedConfiguration,
    .AdvancedConfigurationSettingNames = fr_AdvancedConfigurationSettingNames,
    .AdvancedConfigurationSettingDescriptions =
        fr_AdvancedConfigurationSettingDescriptions};

#endif  // OSSM_SOFTWARE_FR_H
