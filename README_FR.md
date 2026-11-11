# Curve Pilot autonome — Prototype 0.1

Nouvelle base d'hébergement minimale. Ce projet est distinct de Curve Mini, qui pilote les macros d'Ableton par MIDI, et des anciens essais V2/V2.1 de Curve Pilot. Aucun ancien moteur audio n'est réutilisé.

## But de ce premier test

Ouvrir UN instrument VST3 sous Windows, jouer au clavier MIDI USB ou au clavier à l'écran, entendre sa sortie stéréo, et observer la charge. Aucun effet, aucune automation, aucun séquenceur, aucune chaîne audio complexe. Ne pas lancer Ableton pour utiliser ce programme.

Le moteur utilise le lecteur standard JUCE AudioProcessorPlayer. Il appelle le seul instrument une fois par tampon complet du périphérique. Aucun découpage en paquets de 32 échantillons. L'affichage est rafraîchi seulement deux fois par seconde. Le changement d'instrument suspend la sortie audio ; les interactions ordinaires avec l'interface ne suspendent pas le moteur.

## Compiler — méthode habituelle GitHub

1. Dézipper les sources. Envoyer leur CONTENU à la racine d'un dépôt dédié, avec les dossiers Source et tests. CMakeLists.txt doit être à la racine.
2. Si le dossier caché .github n'est pas envoyé par le navigateur, créer .github/workflows/build.yml dans GitHub et y coller le contenu de WORKFLOW_A_COPIER.yml, fourni à côté.
3. Dans Actions, lancer « Curve Pilot Autonome Prototype 0.1 Windows ».
4. À la réussite, télécharger l'artefact « Curve-Pilot-Autonome-0.1-Windows-x64 », le dézipper et lancer « Curve Pilot Autonome 0.1.exe ».

C'est une application Windows, pas un fichier .vst3 à copier dans Common Files. Les VST3 chargés à l'intérieur restent leurs installations normales, avec leurs licences et leurs banques.

## Premier essai

1. Fermer Ableton et les autres logiciels qui utilisent exclusivement la carte son.
2. Lancer le programme. Cliquer Audio / MIDI pour choisir la sortie audio et cocher l'entrée du clavier MIDI USB. Les réglages sont visibles : fréquence et taille du tampon. Le prototype utilise les pilotes disponibles par défaut dans JUCE sous Windows ; ASIO n'est pas inclus dans ce paquet.
3. Cliquer Charger un instrument VST3. Le sélecteur ouvre normalement C:\Program Files\Common Files\VST3. Choisir le fichier ou dossier VST3 COMPLET de Serum 2 ou d'un autre instrument installé.
4. L'interface originale du synthé s'ouvre dans sa propre fenêtre native. Le tableau de contrôle et son clavier à l'écran restent séparés. Il n'y a pas de miniature transformée, ni de zoom imbriqué. L'intégration en pages dans une seule fenêtre viendra après validation du moteur ; elle n'est pas implémentée ici.
5. Choisir un preset simple et jouer. Afficher l'instrument rouvre sa fenêtre si elle a été fermée. Couper les notes envoie All Sound Off sur les 16 canaux.
6. Observer Charge audio et Décrochages, ainsi que la taille du tampon. Comparer ensuite d'autres presets et un autre instrument sans ajouter d'autres fonctions.

L'indicateur de charge mesure le temps des callbacks audio rapporté au temps disponible, via AudioDeviceManager. Ce n'est ni la RAM ni un pourcentage global du processeur. Sa valeur n'est pas directement interchangeable avec tous les modes d'affichage d'Ableton. Les décrochages dépendent aussi du pilote et ne constituent pas un compteur universel de tous les craquements audibles.

## Limites de cette base

- Un instrument seulement. Les effets intégrés et VST viendront plus tard.
- Sortie stéréo et aucune entrée audio de l'instrument. Les instruments exigeant une autre configuration de bus sont refusés dans cette première version.
- Pas de sauvegarde de session/preset ni de mémorisation des réglages audio. Les réglages internes restent ceux du synthé pendant l'exécution ; fermer le programme les perd si le preset n'a pas été enregistré dans le synthé.
- Pas d'horloge musicale, de bouton Lecture ou de tempo transmis au synthé : le test concerne le jeu par notes, pas ses séquenceurs synchronisés.
- L'éditeur garde sa taille native. S'il est trop grand pour l'écran, régler sa taille dans le synthé lorsque cette fonction existe. Le travail de redimensionnement/DPI reste à valider, notamment à 300 % Windows.
- La recherche et la création de certains VST peuvent prendre du temps ; le code ne promet pas une isolation contre les plantages d'un VST tiers.
- Pas d'enregistrement audio, d'automations ni de contrôle MIDI appris dans l'hôte.

## Validation exacte au moment de la livraison

Vérification de syntaxe C++ de Main.cpp et de tests/engine_tests.cpp avec JUCE 8.0.6 : réussie. Vérification de la structure du workflow et de l'archive : réussie.

Un test de moteur est fourni et exécuté par le workflow Windows avant production de l'exécutable : il vérifie un seul appel pour chacun des tampons de 64, 128, 256, 512 et 1024 échantillons, la conservation de la position d'une note MIDI en fin de tampon, la sortie stéréo, la préparation/libération et le silence sans instrument.

Ce test JUCE complet n'a pas été exécuté localement : l'environnement de livraison ne possède pas la chaîne de compilation Windows. Le workflow Windows doit encore compiler et exécuter ces tests. L'ouverture réelle de Serum, le MIDI USB, l'audio, les fenêtres à 300 % et la consommation sur le PC utilisateur ne sont PAS encore validés. Ne pas considérer « léger » comme une performance déjà mesurée.
