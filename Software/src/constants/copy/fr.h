#ifndef OSSM_SOFTWARE_FR_H
#define OSSM_SOFTWARE_FR_H

#include "structs/LanguageStruct.h"
#include <EEPROM.h>


// French strings in PROGMEM
const char fr_DeepThroatTrainerSync[] PROGMEM = "DeepThroat Sync";
const char fr_Error[] PROGMEM = "Erreur";
const char fr_GetHelp[] PROGMEM = "Aide";
const char fr_GetHelpLine1[] PROGMEM = "Sur Discord,";
const char fr_GetHelpLine2[] PROGMEM = "ou GitHub";
const char fr_Homing[] PROGMEM = "FR - Homing";
const char fr_HomingTookTooLong[] PROGMEM = "Le homing a pris trop de temps. Veuillez vérifier votre câblage et réessayer.";
const char fr_Idle[] PROGMEM = "Inactif";
const char fr_InDevelopment[] PROGMEM = "Ceci est en développement.";
const char fr_MeasuringStroke[] PROGMEM = "Mesure de la course";
const char fr_NoInternalLoop[] PROGMEM = "Aucun gestionnaire d'affichage implémenté.";
const char fr_Restart[] PROGMEM = "Redémarrage";
const char fr_Settings[] PROGMEM = "Paramètres";
const char fr_SimplePenetration[] PROGMEM = "Pénétration simple";
const char fr_Skip[] PROGMEM = "Quitter ->";
const char fr_Speed[] PROGMEM = "Vitesse";
const char fr_SpeedWarning[] PROGMEM = "Réduisez la vitesse pour commencer à jouer.";
const char fr_StateNotImplemented[] PROGMEM = "État: %u non implémenté.";
const char fr_Stroke[] PROGMEM = "Coup";
const char fr_StrokeEngine[] PROGMEM = "Stroke Engine";
const char fr_StrokeTooShort[] PROGMEM = "Course trop courte. Veuillez vérifier votre courroie d'entraînement.";
const char fr_Pair[] PROGMEM = "Jumeler l'appareil";
const char fr_PairingFailed[] PROGMEM = "L'appairage a échoué. Veuillez réessayer.";
const char fr_PairingInstructions[] PROGMEM = "Entrez le code suivant sur le tableau de bord";
const char fr_Update[] PROGMEM = "Mettre à jour";
const char fr_UpdateMessage[] PROGMEM = "La mise à jour est en cours. Ça peut prendre jusqu'à 60 secondes.";
const char fr_WiFi[] PROGMEM = "Wi-Fi";
const char fr_WiFiSetup[] PROGMEM = "Config. Wi-Fi";
const char fr_WiFiSetupLine1[] PROGMEM = "Se connecter à";
const char fr_WiFiSetupLine2[] PROGMEM = "'Ossm Setup'";
const char fr_YouShouldNotBeHere[] PROGMEM = "Vous ne devriez pas être ici.";

// Stroke Engine Descriptions
const char fr_StrokeEngineDescriptions_0[] PROGMEM = "Accélération, roulement, décélération également répartis ; sans sensation.";
const char fr_StrokeEngineDescriptions_1[] PROGMEM = "La vitesse change avec la sensation ; équilibre les coups rapides.";
const char fr_StrokeEngineDescriptions_2[] PROGMEM = "La sensation varie l'accélération ; de robotique à progressive.";
const char fr_StrokeEngineDescriptions_3[] PROGMEM = "Alternance de coups pleins et à demi-profondeur ; la sensation affecte la vitesse.";
const char fr_StrokeEngineDescriptions_4[] PROGMEM = "La profondeur des coups augmente à chaque cycle ; la sensation définit le nombre.";
const char fr_StrokeEngineDescriptions_5[] PROGMEM = "Pauses entre les coups ; la sensation ajuste la longueur.";
const char fr_StrokeEngineDescriptions_6[] PROGMEM = "Modifie la longueur, maintient la vitesse ; la sensation influe sur la direction.";

// Stroke Engine Names
const char fr_StrokeEngineNames_0[] PROGMEM = "Simple Stroke";
const char fr_StrokeEngineNames_1[] PROGMEM = "Teasing Pounding";
const char fr_StrokeEngineNames_2[] PROGMEM = "Robo Stroke";
const char fr_StrokeEngineNames_3[] PROGMEM = "Half'n'Half";
const char fr_StrokeEngineNames_4[] PROGMEM = "Deeper";
const char fr_StrokeEngineNames_5[] PROGMEM = "Stop'n'Go";
const char fr_StrokeEngineNames_6[] PROGMEM = "Insist";


// TODO: Requires validation by a native french speaker.
//  These have been translated by Google Translate.
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
        .Pair = fr_Pair,
        .PairingFailed = fr_PairingFailed,
        .PairingInstructions = fr_PairingInstructions,
        .Update = fr_Update,
        .UpdateMessage = fr_UpdateMessage,
        .WiFi = fr_WiFi,
        .WiFiSetup = fr_WiFiSetup,
        .WiFiSetupLine1 = fr_WiFiSetupLine1,
        .WiFiSetupLine2 = fr_WiFiSetupLine2,
        .YouShouldNotBeHere = fr_YouShouldNotBeHere,
        .StrokeEngineDescriptions = {
                fr_StrokeEngineDescriptions_0,
                fr_StrokeEngineDescriptions_1,
                fr_StrokeEngineDescriptions_2,
                fr_StrokeEngineDescriptions_3,
                fr_StrokeEngineDescriptions_4,
                fr_StrokeEngineDescriptions_5,
                fr_StrokeEngineDescriptions_6
        },
        .StrokeEngineNames = {
                fr_StrokeEngineNames_0,
                fr_StrokeEngineNames_1,
                fr_StrokeEngineNames_2,
                fr_StrokeEngineNames_3,
                fr_StrokeEngineNames_4,
                fr_StrokeEngineNames_5,
                fr_StrokeEngineNames_6
        }
};


#endif  // OSSM_SOFTWARE_FR_H
