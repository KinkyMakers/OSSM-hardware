#ifndef OSSM_SOFTWARE_FR_H
#define OSSM_SOFTWARE_FR_H

#include "structs/LanguageStruct.h"

// TODO: Requires validation by a native french speaker.
//  These have been translated by Google Translate.
// French copy - All strings stored in PROGMEM
static const char fr_DeepThroatTrainerSync[] PROGMEM = "DeepThroat Sync";
static const char fr_Error[] PROGMEM = "Erreur";
static const char fr_GetHelp[] PROGMEM = "Aide";
static const char fr_GetHelpLine1[] PROGMEM = "Sur Discord,";
static const char fr_GetHelpLine2[] PROGMEM = "ou GitHub";
static const char fr_Homing[] PROGMEM = "FR - Homing";
static const char fr_HomingTookTooLong[] PROGMEM =
    "Le homing a pris trop de temps.Veuillez vérifier votre câblage et "
    "réessayer.";
static const char fr_Idle[] PROGMEM = "Inactif";
static const char fr_InDevelopment[] PROGMEM = "Ceci est en développement.";
static const char fr_MeasuringStroke[] PROGMEM = "Mesure de la course";
static const char fr_NoInternalLoop[] PROGMEM =
    "Aucun gestionnaire d'affichage implémenté.";
static const char fr_Restart[] PROGMEM = "Redémarrage";
static const char fr_Settings[] PROGMEM = "Paramètres";
static const char fr_SimplePenetration[] PROGMEM = "Pénétration simple";
static const char fr_Skip[] PROGMEM = "Quitter ->";
static const char fr_Speed[] PROGMEM = "Vitesse";
static const char fr_SpeedWarning[] PROGMEM =
    "Réduisez la vitesse pour commencer à jouer.";
static const char fr_StateNotImplemented[] PROGMEM = "État: %u non implémenté.";
static const char fr_Streaming[] PROGMEM = "Streaming (beta)";
static const char fr_Stroke[] PROGMEM = "Coup";
static const char fr_StrokeEngine[] PROGMEM = "Stroke Engine";
static const char fr_StrokeTooShort[] PROGMEM =
    "Course trop courte. Veuillez vérifier votre courroie d'entraînement.";
static const char fr_Pairing[] PROGMEM = "Appairage";
static const char fr_Update[] PROGMEM = "Mettre à jour";
static const char fr_UpdateMessage[] PROGMEM =
    "La mise à jour est en cours. Ça peut prendre jusqu'à 60 secondes.";
static const char fr_WiFi[] PROGMEM = "Wi-Fi";
static const char fr_WiFiSetup[] PROGMEM = "Config. Wi-Fi";
static const char fr_WiFiSetupLine1[] PROGMEM = "Se connecter à";
static const char fr_WiFiSetupLine2[] PROGMEM = "'Ossm Setup'";
static const char fr_YouShouldNotBeHere[] PROGMEM =
    "Vous ne devriez pas être ici.";

static const char fr_StrokeEngineDescriptions_0[] PROGMEM =
    "Accélération, roulement, décélération également répartis ;";
static const char fr_StrokeEngineDescriptions_1[] PROGMEM =
    "La vitesse change avec la sensation ; équilibre les coups rapides.";
static const char fr_StrokeEngineDescriptions_2[] PROGMEM =
    "La sensation varie l'accélération ; de robotique à progressive.";
static const char fr_StrokeEngineDescriptions_3[] PROGMEM =
    "Alternance de coups pleins et à demi-profondeur ; la sensation affecte la "
    "vitesse.";
static const char fr_StrokeEngineDescriptions_4[] PROGMEM =
    "La profondeur des coups augmente à chaque cycle ; la sensation définit le "
    "nombre.";
static const char fr_StrokeEngineDescriptions_5[] PROGMEM =
    "Pauses entre les coups ; la sensation ajuste la longueur.";
static const char fr_StrokeEngineDescriptions_6[] PROGMEM =
    "";
static const char fr_StrokeEngineDescriptions_7[] PROGMEM =
    "";
static const char fr_StrokeEngineDescriptions_8[] PROGMEM =
    "";
static const char fr_StrokeEngineDescriptions_9[] PROGMEM =
    "";

static const char fr_StrokeEngineNames_0[] PROGMEM = "Simple Stroke";
static const char fr_StrokeEngineNames_1[] PROGMEM = "Teasing Pounding";
static const char fr_StrokeEngineNames_2[] PROGMEM = "Robo Stroke";
static const char fr_StrokeEngineNames_3[] PROGMEM = "Half'n'Half";
static const char fr_StrokeEngineNames_4[] PROGMEM = "Deeper";
static const char fr_StrokeEngineNames_5[] PROGMEM = "Stop'n'Go";
static const char fr_StrokeEngineNames_6[] PROGMEM = "Insist";
static const char fr_StrokeEngineNames_7[] PROGMEM = "Progressive Stroke";
static const char fr_StrokeEngineNames_8[] PROGMEM = "Random Stroke";
static const char fr_StrokeEngineNames_9[] PROGMEM = "Go to Point";

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
    .Streaming = fr_Streaming,
    .Stroke = fr_Stroke,
    .StrokeEngine = fr_StrokeEngine,
    .StrokeTooShort = fr_StrokeTooShort,
    .Pairing = fr_Pairing,
    .Update = fr_Update,
    .UpdateMessage = fr_UpdateMessage,
    .WiFi = fr_WiFi,
    .WiFiSetup = fr_WiFiSetup,
    .WiFiSetupLine1 = fr_WiFiSetupLine1,
    .WiFiSetupLine2 = fr_WiFiSetupLine2,
    .YouShouldNotBeHere = fr_YouShouldNotBeHere,
    .StrokeEngineDescriptions = {fr_StrokeEngineDescriptions_0,
                                 fr_StrokeEngineDescriptions_1,
                                 fr_StrokeEngineDescriptions_2,
                                 fr_StrokeEngineDescriptions_3,
                                 fr_StrokeEngineDescriptions_4,
                                 fr_StrokeEngineDescriptions_5,
                                 fr_StrokeEngineDescriptions_6,
                                 fr_StrokeEngineDescriptions_7,
                                 fr_StrokeEngineDescriptions_8,
                                 fr_StrokeEngineDescriptions_9
    },
    .StrokeEngineNames = {fr_StrokeEngineNames_0,
                          fr_StrokeEngineNames_1,
                          fr_StrokeEngineNames_2,
                          fr_StrokeEngineNames_3,
                          fr_StrokeEngineNames_4,
                          fr_StrokeEngineNames_5,
                          fr_StrokeEngineNames_6,
                          fr_StrokeEngineNames_7,
                          fr_StrokeEngineNames_8,
                          fr_StrokeEngineNames_9
    }
};

#endif  // OSSM_SOFTWARE_FR_H
