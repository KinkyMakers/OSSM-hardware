#ifndef OSSM_SOFTWARE_FR_H
#define OSSM_SOFTWARE_FR_H

#include "structs/LanguageStruct.h"

// TODO: Requires validation by a native french speaker.
//  These have been translated by Google Translate.
static const LanguageStruct fr = {
    .DeepThroatTrainerSync = "DeepThroat Sync",
    .Error = "Erreur",
    .GetHelp = "Aide",
    .GetHelpLine1 = "Sur Discord,",
    .GetHelpLine2 = "ou GitHub",
    .Homing = "FR - Homing",
    .HomingTookTooLong =
        "Le homing a pris trop de temps.Veuillez vérifier votre câblage et "
        "réessayer.",
    .Idle = "Inactif",
    .InDevelopment = "Ceci est en développement.",
    .MeasuringStroke = "Mesure de la course",
    .NoInternalLoop = "Aucun gestionnaire d'affichage implémenté.",
    .Restart = "Redémarrage",
    .Settings = "Paramètres",
    .SimplePenetration = "Pénétration simple",
    .Skip = "Quitter ->",
    .Speed = "Vitesse",
    .SpeedWarning = "Réduisez la vitesse pour commencer à jouer.",
    .StateNotImplemented = "État: %u non implémenté.",
    .Stroke = "Coup",
    .StrokeEngine = "Stroke Engine",
    .StrokeTooShort =
        "Course trop courte. Veuillez vérifier votre courroie d'entraînement.",
    .Update = "Mettre à jour",
    .UpdateMessage =
        "La mise à jour est en cours. Ça peut prendre jusqu'à 60 secondes.",
    .WiFi = "Wi-Fi",
    .WiFiSetup = "Config. Wi-Fi",
    .WiFiSetupLine1 = "Se connecter à",
    .WiFiSetupLine2 = "'Ossm Setup'",
    .YouShouldNotBeHere = "Vous ne devriez pas être ici.",
    .StrokeEngineDescriptions = {
        "Accélération, roulement, décélération également répartis ; sans sensation.",
        "La vitesse change avec la sensation ; équilibre les coups rapides.",
        "La sensation varie l'accélération ; de robotique à progressive.",
        "Alternance de coups pleins et à demi-profondeur ; la sensation affecte la vitesse.",
        "La profondeur des coups augmente à chaque cycle ; la sensation définit le nombre.",
        "Pauses entre les coups ; la sensation ajuste la longueur.",
        "Modifie la longueur, maintient la vitesse ; la sensation influe sur la direction.",
    },
    .StrokeEngineNames = {
        "Simple Stroke",
        "Teasing Pounding",
        "Robo Stroke",
        "Half'n'Half",
        "Deeper",
        "Stop'n'Go",
        "Insist",
    }
};


#endif  // OSSM_SOFTWARE_FR_H
