# 7. Code modulaire

**Statut : fait pour les payloads du 3GS (2026-10-15).** Ils sont partis d'un seul fichier, `tools/payloads/trace.c`,
avec les adresses du 3GS écrites en dur, ce qui ne survit pas à une deuxième puce. Le code est maintenant découpé en
`hal/` (portable), `soc/` (constantes par puce) et `trace.c` (le payload). La vérification de non-régression est un
rendu émulé de chaque étape, identique avant et après le découpage (EMU), puis `trace.bin` construit depuis le
code découpé a tourné sur le téléphone et a affiché le même écran qu'avant (**DEVICE**, 2026-10-15). L'en-tête A4 existe mais est volontairement inutilisable tant qu'il n'est pas vérifié.

![Couches du code, du matériel aux Mods](../img/code-layers.svg)

## Les couches

| Couche | Chemin | Contient | Ne contient pas |
|---|---|---|---|
| **HAL** | `tools/payloads/hal/` | console (texte à l'écran), `exc` (gestionnaires d'exceptions), `vic` (interruptions), `result` (valeurs publiées vers l'hôte), `gpio.h` et `clock.h` (accès aux mots de pins et de portes d'horloge), `i2c_gpio` (I2C par bit-bang et accès aux registres du PMU). Code ARMv7 / Cortex-A8 portable. | **aucune adresse** |
| **Constantes SoC** | `tools/payloads/soc/s5l8920.h` | les adresses, tailles et masques du 3GS, chaque ligne avec son étiquette de preuve | de la logique |
| | `tools/payloads/soc/s5l8930.h` | les constantes de l'A4, **SOURCE seulement** (lues dans OpeniBoot, jamais sur un téléphone) | de la logique |
| **Sélecteur** | `tools/payloads/soc.h` | choisit l'en-tête selon `-DSOC_<NOM>` (`make SOC=s5l8920`) | |
| **Payload** | `tools/payloads/trace.c` (entrée et étapes communes) et `tools/payloads/stages/NN_nom.inc` (un fichier par expérience) | l'image de mise en route : assemble HAL et constantes SoC | de chiffre propre à une puce |

Le HAL demande à `soc.h` des choses comme « où est le framebuffer » et « où est le contrôleur
d'interruptions ». Il n'écrit jamais un chiffre lui-même : le même code HAL **se compile** donc pour toute puce
qui a un fichier de constantes. Se compiler n'est pas se comporter pareil : voir la section suivante.

## Quatre sortes de connaissance, quatre endroits

Une différence entre puces n'est pas toujours une constante, et un comportement vu sur le 3GS n'est pas une
propriété de l'architecture. Le code sépare quatre sortes de connaissance, parce qu'elles n'ont ni la même durée
de vie ni la même preuve :

| Niveau | Ce que c'est | Où ça vit aujourd'hui | Se transporte vers une autre puce ? |
|---|---|---|---|
| **Cœur ARM** | exceptions et vecteurs, caches, barrières, identification du CPU | `hal/entry.c`, `hal/exc.c` | oui, pour la même famille de cœur ; **prouvé seulement sur le Cortex-A8 du 3GS** |
| **SoC** | contrôleurs et leurs registres : contrôleur d'interruptions, bloc GPIO, portes d'horloge, timer, contrôleurs I2C et SPI | `soc/<puce>.h` et le HAL qui ne lit que des `SOC_*` (`gpio.h`, `clock.h`, `vic.c`, `i2c_gpio.c`) | un nouvel en-tête, revérifié ; un contrôleur qui se comporte autrement demande du code, pas seulement une constante |
| **Carte** | câblage et alimentation d'un produit : quelle broche est le reset du tactile, l'adresse I2C du PMU, quelle porte alimente quoi | `soc/<puce>.h` **pour l'instant, mêlé au SoC** (à séparer quand une seconde carte existera) | non : c'est un fait sur un modèle de téléphone |
| **Expérience** | une séquence à lancer et à prouver : préconditions, lectures, écritures, restauration, preuve | `tools/payloads/stages/NN_nom.inc` (un `STAGE`), et le dossier qu'elle produit | non : elle est datée et liée à sa preuve |

Règles qui en découlent :

- **« Portable » veut dire que le code se compile pour une autre puce, pas que le comportement y est confirmé.** Chaque fait
  de SoC ou de carte garde son étiquette de preuve et dit sur quel appareil il a été vu. Le HAL n'affirme rien d'une puce où
  personne ne l'a lancé.
- **Une constante ne suffit pas toujours.** Si deux puces diffèrent par la façon dont un contrôleur fonctionne (pas par son
  adresse), c'est un nouveau pilote ou une variante nommée, pas un `if` caché ni un nombre.
- **Ne pas mettre le câblage d'une carte dans le HAL.** L'adresse `0x74` du PMU et les broches de l'accéléromètre sont des
  faits sur la carte de l'iPhone 3GS (N88). Ils sont dans `soc/s5l8920.h` aujourd'hui seulement parce qu'il n'y a qu'une
  carte ; le fichier de carte séparé arrivera avec la seconde, pour ne pas deviner une frontière trop tôt.

## Les règles

1. **Une constante entre dans `soc/<puce>.h` avec une étiquette de preuve.** `DEVICE` seulement si elle a
   été lue sur un téléphone (voir l'[index](README.md)) ; sinon `SOURCE` ou `HYPOTHESIS`.
2. **Un en-tête non vérifié ne se compile pas par défaut.** `s5l8930.h` ne contient que des valeurs
   **SOURCE**, donc le construire est refusé sauf avec `ALLOW_UNVERIFIED=1`. Une réussite silencieuse sur
   une puce non testée est la panne que ce projet craint le plus ([notes de terrain](05-field-notes.md)).
3. **Une nouvelle puce veut dire un nouveau `soc/<puce>.h`, revérifié constante par constante sur
   l'appareil. Cela ne veut pas dire une réécriture.** Si le HAL a besoin d'un `if` propre à une puce,
   demander d'abord si c'est une constante manquante ; si le contrôleur lui-même se comporte autrement, c'est un nouveau
   pilote ou une variante nommée (voir les quatre niveaux plus haut).
4. **Tout ce qui n'est pas une adresse mais change quand même d'une puce à l'autre** (les offsets de patch
   de l'iBSS, l'adresse d'`env_set`) va aussi dans le fichier de constantes, pas dans le HAL.

## Ce que ça apporte

- Le [banc iPhone 4](06-chips.md) devient « écrire `s5l8930.h`, construire avec `ALLOW_UNVERIFIED=1`, lire
  l'écran ». Si une console apparaît sans rien changer d'autre, le découpage est validé.
- Les [Mods](04-mods.md) restent portables au niveau source : ils appellent le HAL et ne voient jamais une adresse, donc un
  Mod **se compile** pour toute puce qui a un fichier de constantes. Qu'il **se comporte** pareil n'est établi que pour les puces où il a été lancé.
- Relire le portage d'une puce, c'est relire un petit en-tête.

## Construire

```sh
cd tools/payloads
make trace.bin                           # puce par défaut : le 3GS
make trace.bin SOC=s5l8930 ALLOW_UNVERIFIED=1   # forme cible, A4 : non vérifié, attendez-vous à corriger des constantes
```

Le build d'une puce non vérifiée s'arrête sur un `#error` ; avec `ALLOW_UNVERIFIED=1` il échoue ensuite sur
chaque constante à trouver (`SOC_SCRATCH`, `SOC_ENV_SET`...), ce qui donne la liste de ce qui reste à vérifier.

## Vérifier un remaniement du HAL

`make -C tools/payloads sums` compile chaque étape et affiche un sha256 par binaire. À lancer avant et après un
changement censé garder le comportement, puis à comparer : les étapes que le changement ne touche pas doivent rester
identiques à l'octet, et celles qu'il touche doivent être lancées sur le téléphone. Le découpage du 2026-10-20
(GPIO, portes d'horloge et I2C sortis de `trace.c`) a été vérifié ainsi (40 binaires sur 49 identiques ; les 9 qui
utilisent l'I2C ou le PMU restent à lancer sur un appareil). **État : compile, pas encore lancé sur l'appareil pour
ces 9.**

## Un fichier par expérience (`stages/`)

Jusqu'au 2026-10-21, chaque expérience était un bloc `#if STAGE == N` dans l'unique fonction `payload_main()` de `trace.c`, ce qui rendait la revue
d'une expérience isolée difficile. Chaque bloc vit désormais dans `tools/payloads/stages/NN_nom.inc`, nommé d'après le binaire qu'il produit
(`47_gate_survey.inc` produit `trace_gate_survey.bin`), et `trace.c` l'inclut au même endroit. Les premières lignes de chaque fichier donnent le
numéro de l'étape et les binaires. Le Makefile reconstruit un payload dès qu'un `stages/*.inc` change : une expérience modifiée n'est jamais
construite à partir d'un objet périmé.

Cette étape est volontairement mécanique. Le compilateur voit le même texte qu'avant, et `make -C tools/payloads sums` donne les **mêmes 49 sha256**
avant et après : le découpage n'a changé aucun binaire, donc rien n'est à relancer sur un téléphone à cause de lui. Les noms de binaires sont inchangés,
donc les commandes et les dossiers de preuve qui les citent restent valables.

Ce qu'il ne fait **pas** encore : les fichiers sont toujours des blocs d'une même fonction et partagent ses variables locales, ce ne sont pas encore
des unités indépendantes. L'étape suivante fait de chacun une fonction à part (préconditions, lectures, écritures, restauration, référence de preuve)
derrière une entrée commune qui initialise la console, l'appelle et publie le résultat. Celle-là change les binaires : elle se fait une expérience à
la fois, avec un passage sur le téléphone.

