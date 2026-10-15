# Micro noyau + driver tactile sur iPhone 3GS (S5L8920) : étude de faisabilité

Research date: 2026-10-14. Étude menée par un autre LLM à partir d'un prompt
construit pour cette session (web uniquement ; rien revérifié sur le matériel
pendant cette recherche. Seuls le dump ROM et le passage en Recovery via
`boot_ibss()`, déjà faits et documentés dans [`docs/en/01-boot-chain.md`](../en/01-boot-chain.md),
sont pris comme acquis).

## Tension avec la décision précédente, tranchée le 2026-10-14

`docs/research/2026-10-14-pre-a5-limera1n.md` concluait initialement : « Treat
them [3GS, A4] as boot-chain test devices, not realistic OS port targets
today » et la feuille de route reprenait la même position. **Décision prise le
2026-10-14 : révoquée.** Le 3GS devient une cible réelle de micro noyau +
driver, sur la base d'OpeniBoot comme environnement de dev. Reporté dans
`docs/research/2026-10-14-pre-a5-limera1n.md` et dans la feuille de route.

## Bottom line

OpeniBoot a plus de support 3GS que supposé : cible `s5l8920`/`iPhone3GS`
explicite, pilotes compilés (horloge, MMU, IRQ, timers, GPIO, UART, I²C, SPI,
LCD/MIPI DSI, USB) et une trace 2015 d'un environnement interactif (console
ACM) tournant réellement sur un 3GS physique. L'affichage est la piste la
plus solide (fichier `clcd.c` spécifique au S5L8920, RGB565/RGB888). Le
tactile est le point faible : le contrôleur du 3GS n'est **pas** celui du 3G
(Zephyr2, documenté), et aucune datasheet ni driver Linux confirmé n'a été
trouvé pour le composant du 3GS lui-même.

## OpeniBoot / iDroid : ce qui existe pour le S5L8920

- Plateforme `s5l8920` + cible `iphone3gs` dans le dépôt OpeniBoot/iDroid
  (github.com/iDroidProject). Pilotes compilés pour cette cible : horloges,
  MMU, interruptions, timers, GPIO, UART, I²C, SPI, contrôleur LCD + MIPI
  DSI, modules USB.
- Trace publiée en 2015 : OpeniBoot 0.3 démarré sur un 3GS réel, console ACM
  accessible, une commande traitée avec succès. Le problème rapporté
  concernait une installation persistante sur NAND, hors périmètre
  tethered de Trognon.
- README historique : images distinctes selon le point d'entrée (Recovery
  ou DFU). Point non démontré : que l'iBSS patché actuel de Trognon puisse
  charger une image OpeniBoot sans adapter le contrat de passage de relais.
- `iDroid-kernel` et `kernel_common` : dépôts de pilotes Linux portés,
  valeur d'archive à auditer, pas de preuve d'un système plus complet sur
  3GS trouvée.
- pongoOS : référence d'organisation (runtime minimal, console, chaînage
  vers un autre noyau), pas un portage utilisable tel quel sur ARMv7.

## Tactile : composant non confirmé, protocole non documenté

- iPhone 3G (génération précédente) : pilote Zephyr2 annoncé fonctionnel par
  David Wang en avril 2010. OpeniBoot garde `multitouch-z1.c` et
  `multitouch-z2.c` dans la plateforme S5L8900, **mais la configuration
  3GS inspectée ne sélectionne pas ces fichiers**. Zephyr2 n'est donc pas
  acquis pour le 3GS.
- Composant du 3GS (piste, pas confirmée sur l'exemplaire réel) : contrôleur
  Texas Instruments, marquage Apple 343S0464, die F761586C ; correspondance
  343S0464/343S0487 selon une analyse TechInsights. Des illustrations
  publiques confondent par ailleurs les générations de composants : à
  vérifier contre l'appareil physique (ouverture, FCC, teardown) avant de
  s'appuyer dessus.
- Aucune datasheet publique exploitable du protocole, aucun driver Linux
  confirmé pour ce composant précis. `bcm5974` (trackpad USB MacBook) n'a
  pas de lien de compatibilité démontré avec l'iPhone. Le nom seul ne
  prouve rien.
- Inconnues à résoudre avant tout code : bus (I²C ou SPI probable, non
  confirmé pour ce composant), reset/IRQ, séquence d'init, firmware
  éventuel à charger, calibration, format des rapports.

## Architecture minimale proposée

Un moniteur bare-metal avec boucle d'événements, pas un micro noyau complet
avec isolation de process + IPC : ce dernier ajouterait du travail avant de
toucher aux pilotes réels.

| Couche | Contenu initial |
|---|---|
| Entrée | stub assembleur, pile réservée, init `.bss`, capture de l'état CPU reçu |
| Diagnostic | journal circulaire en RAM, exceptions, identifiant de build |
| Support matériel | timers, GPIO, console, puis écran et SPI/I²C |
| Services | allocation simple, délais bornés, files d'événements |
| Application | texte + rectangles, puis événements DOWN/MOVE/UP |
| Passage de relais | contrat explicite pour charger un autre noyau ensuite |

Pour le premier jalon : **préserver l'état matériel laissé par
l'iBSS/Recovery** (ne pas couper la MMU, ne pas reprogrammer PLL/mémoire à
l'aveugle) et définir le contrat d'entrée (adresse de chargement, point
d'entrée, mode ARM/Thumb, état des registres/IRQ/MMU/caches, zones RAM
réservées) ; Recovery est un protocole de communication, pas ce contrat.

OpeniBoot fournit une séquence d'init S5L8920 à étudier fonction par
fonction (prépa ARM/MMU, tâches, mémoire/alimentation, horloges, IRQ, GPIO,
timers, périphériques), y compris des opérations MMU que ses propres
auteurs documentent imparfaitement. Source historique, pas une spec
constructeur.

Écran avant tactile : choix justifié ici spécifiquement (pas seulement par
analogie avec Hoolock/A7+) ; `clcd.c` du S5L8920 contient déjà une init de
panneau et des framebuffers RGB565/RGB888 propres à ce SoC.

## Jalons (estimations de planification, pas des mesures)

| Jalon | Preuve attendue | Estimation |
|---|---|---|
| Reconstruction OpeniBoot | image compilée depuis une révision figée, sans flash | jours à semaines |
| Exécution RAM | marqueur Trognon relu, retour reproductible au boot initial | réaliste, dépend du contrat iBSS |
| Console | échange fiable avec le Mac ; UART seulement si accessible | jours à semaines |
| Affichage | mire puis texte, renouvelés par notre code | semaines |
| Transport tactile | réponse valide du contrôleur, ID ou rapport brut | exploratoire, délai incertain |
| Tactile utilisable | coordonnées stables, bouton cliquable | potentiellement des mois |
| Réemploi durable | charge, température, veille, endurance | chantier distinct, après démonstration |

## Premier pas concret proposé (par l'étude)

Reconstruire OpeniBoot comme environnement de dev (pas un point de départ de
code neuf), faire afficher "Trognon" + hash de build + compteur animé depuis
une image chargée en RAM uniquement, puis bouton physique, puis tactile.
Sépare clairement l'écran hérité du chargeur de l'exécution de notre propre
code, et permet de trancher tôt si le 3GS est une plateforme de réemploi
viable ou surtout un terrain de recherche.

## Non vérifié

- Tout ce qui précède est web uniquement ; aucune ligne n'a été revérifiée
  contre l'exemplaire physique de ce projet pendant cette étude.
- Identification exacte du contrôleur tactile sur CET exemplaire (marquage
  343S0464 à confirmer par ouverture/lecture directe, pas seulement par
  analogie avec un composant vu ailleurs).
- Compatibilité du contrat de passage de relais entre l'iBSS (Trognon) et l'image
  OpeniBoot : pas démontrée, à construire et tester.
- Bus, protocole, et tout détail d'initialisation du contrôleur tactile du
  3GS : aucune source primaire trouvée, tout est à reconstruire par
  reverse-engineering si on va jusque-là.
