# Curve Pilot autonome 0.2

Application Windows x64 pour jouer un instrument VST3, automatiser ses paramètres avec douze courbes et enregistrer le résultat en WAV stéréo 24 bits.

## Jouer et dessiner

1. Ouvrir **Audio / MIDI** pour choisir la sortie et activer le clavier USB. Le clavier à l'écran est également disponible.
2. Cliquer sur **Charger un instrument VST3** et sélectionner le fichier ou dossier `.vst3` du synthé installé. La vue **Instrument** affiche son éditeur avec défilement.
3. Dans **Courbes**, choisir une ligne, activer **Active**, puis sélectionner un paramètre via le filtre ou **Dernier potard**, après avoir touché le réglage dans la vue Instrument. Le filtre fonctionne aussi quand le VST3 ne signale pas les gestes.
4. Choisir une forme ou dessiner. Régler la plage et la durée de la ligne. **Lecture** lance les mouvements et **Début** remet le transport à zéro. Les notes restent jouables en pause.
5. Choisir **Mesures WAV** et cliquer sur **Capturer la boucle**. La capture repart du début, à tempo fixe. Pendant la capture, jouer les notes ou utiliser un arpégiateur du synthé. Quand la durée est atteinte, cliquer sur **Enregistrer WAV** pour choisir le fichier.

Les durées des courbes sont indépendantes. Pour que leurs cycles coïncident avec les bords du WAV, choisir une durée de capture multiple de leurs durées. Les courbes sont conservées pendant la session. Un changement d'instrument efface leurs destinations.

## Compiler et tester

Dans GitHub Actions, lancer **Curve Pilot Autonome Prototype 0.2 Windows** sur la branche du prototype. Télécharger **Curve-Pilot-Autonome-0.2-Windows-x64** et lancer **Curve Pilot Autonome 0.2.exe**.

Le moteur appelle le VST3 une fois par tampon. Pendant la lecture, il évalue les lignes actives une fois par tampon et évite de renvoyer les valeurs inchangées. La capture utilise une mémoire préallouée, limitée à 16 millions d'échantillons par canal. L'écriture du WAV se fait après la capture, en dehors du moteur audio.

Pour mesurer le surcoût réel, utiliser le même instrument, preset, jeu MIDI, fréquence et taille de tampon avec zéro, une puis douze courbes actives. Relever la charge audio et les décrochages affichés. Vérifier aussi le dessin, le défilement du VST3 et l'assignation à la mise à l'échelle Windows utilisée.
