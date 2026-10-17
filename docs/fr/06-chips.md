# 6. Support par puce

Trognon se construit **de bas en haut à partir de l'iPhone 3GS**, puis s'élargit famille de puces par
famille de puces. Cette page dit, pour chaque famille, ce qu'on sait, comment elle démarre, et ce qui se
reporte depuis la voisine. Étiquettes de preuve comme dans l'[index](README.md). Nous n'avons touché
que le 3GS : tout le reste est **SOURCE** (lu dans nos notes de recherche) ou **HYPOTHESIS**.

## Deux régimes, pas une seule échelle

Les familles ne forment pas une ligne continue. Il y a une rupture entre l'iPhone 4 et l'iPhone 4S.

| | **Régime A : avant A5** (3GS, iPhone 4) | **Régime B : A5 et après** (4S et suivants) |
|---|---|---|
| Exploit de boot | limera1n (3GS), SHAtter (A4, selon ipwndfu) : bugs bootrom de 2010, **avant** checkm8 (**SOURCE**) | checkm8 (2019), A5 à A11 (**SOURCE**) |
| Carte auxiliaire | aucune, hôte direct | l'A5 demande un Pico (**SOURCE**) ; les suivantes non |
| Travail d'OS existant | rien sur quoi construire aujourd'hui : OpeniBoot, dormant (**SOURCE**) | Project Cascadia (A5), Hoolock, arbre Asahi, postmarketOS (**SOURCE**) |
| Ce qu'on y fait | **bare metal** : nos propres payloads de boot, pilotes et [Mods](04-mods.md) | **porter un OS** et contribuer en amont, sans forker (**HYPOTHESIS** pour le 4S) |
| Rôle dans Trognon | le laboratoire : tout est prouvé ici d'abord | la destination d'un OS généraliste |

Le 3GS et l'iPhone 4 sont donc dans une même configuration (bare metal, notre code), le 4S et la suite
dans une autre (du travail d'OS existant à rejoindre). Ce qui passe de A à B, c'est la méthode et les
habitudes (niveaux de preuve, idée de la console à l'écran), pas les adresses. Dans le régime A, les deux
téléphones diffèrent par l'outil d'exploit (limera1n contre SHAtter) : et d'anciens textes disent limera1n
pour les deux : la [note de recherche](../research/2026-10-14-pre-a5-limera1n.md) porte la correction.


## Frise, puce par puce

Une carte par génération de puce, dans l'ordre de sortie. Couleur du bord gauche : régime. Fond vert : là où notre code tourne. Contour rouge : le prochain banc.

--8<-- "docs/snippets/timeline.fr.html"

## Recouvrements en un coup d'œil

| | 3GS (S5L8920) | iPhone 4 (A4, S5L8930) | iPhone 4S (A5, S5L8940) | A6 (5, 5c) | A7 à A11 |
|---|---|---|---|---|---|
| Notre statut | **DEVICE** : le code tourne | pas essayé | iPad 2 identifié (**DEVICE**, lecture seule) ; boot pas essayé | pas essayé | pas essayé |
| Cœur CPU | Cortex-A8 r2p1 (**DEVICE**, `MIDR`) | classe Cortex-A8 (**HYPOTHESIS**, lire `MIDR` d'abord) | double cœur, classe Cortex-A9 (**HYPOTHESIS**) | 32 bits (**SOURCE**) | arm64 (**SOURCE**) |
| Jeu d'instructions | ARMv7 32 bits | ARMv7 32 bits | ARMv7 32 bits | ARMv7 32 bits | 64 bits : une autre piste |
| Exploit de boot | limera1n (**DEVICE**) | SHAtter selon ipwndfu (**SOURCE**) | checkm8-a5, demande un Pico (**SOURCE**) | checkm8, natif en USB (**SOURCE**) | checkm8 (**SOURCE**) |
| Carte auxiliaire | aucune | aucune | Pico RP2040 + câble OTG en Y | aucune | aucune |
| Méthode de boot réutilisable ? | la référence | oui : même forme (DFU pwned, iBSS patché, run) | en partie : un chargeur en étapes au lieu de `run` (**HYPOTHESIS**) | en partie | non : pongoOS, m1n1, Linux |
| Code CPU, console d'exceptions | réutilisable entre 3GS et A4 (**HYPOTHESIS**) | réutilisable | réutilisable si ARMv7 (**HYPOTHESIS**) | réutilisable | non réutilisable (arm64) |
| Adresses, écran, iBSS | vérifiés | **à refaire** | **à refaire** | **à refaire** | **à refaire** |
| Travail OS existant | OpeniBoot/iDroid, dormant (**SOURCE**) | partiel dans OpeniBoot, dormant (**SOURCE**) | Project Cascadia sur iPad mini 1 (**SOURCE**) | rien trouvé (**SOURCE**) | Hoolock, arbre Asahi, postmarketOS (**SOURCE**) |

Ce qui se reporte, c'est la *méthode* et le *code au niveau du CPU*. Ce qui ne se reporte jamais, ce sont
les adresses, le chemin d'affichage et les offsets de patch de l'iBSS : on les relit sur chaque puce
(voir l'[organisation du code](07-code-layout.md)).

## iPhone 3GS, S5L8920 : la référence (DEVICE)

- DFU pwned avec limera1n, iBSS patché, `run` en Recovery, payload en `0x41000000` :
  [chaîne de démarrage](01-boot-chain.md). Registres, écran, GPIO et contrôleurs d'interruptions :
  [carte matérielle](03-hardware-map.md). Exceptions : [console](02-exception-console.md).
- Écran 320 x 480, framebuffer `0x4fd00000`.
- Prochain barreau de l'[échelle](04-mods.md) : un timer (l'IRQ déclenchée par logiciel est prouvée).
- OpeniBoot/iDroid est le code lu pour les adresses, dormant depuis 2022
  (**SOURCE**, [recherche](../research/2026-10-14-microkernel-3gs.md)).
- **NAND** : identifié, géométrie et capacité confirmées (Toshiba, 32 Go, deux matrices sur
  CE0/CE1) — [carte matérielle](03-hardware-map.md#nand).
- **Wi-Fi/SDIO** (BCM4325) : adresse, clock-gate, IRQ et les lignes GPIO de reset/réveil sont
  connues, la ligne de reset a été basculée, mais tout registre du bloc lit toujours `0`.
  L'explication la plus probable est architecturale, pas un mauvais décalage : un second
  processeur embarqué (l'« IOP » d'Apple) possède ce bloc, avec son propre petit système qui
  pilote aussi le NAND et un moteur DMA chiffré — voir le
  [rapport sur l'architecture Wi-Fi/IOP](../research/2026-10-16-wifi-iop-architecture.md).
- **Tactile** : aucun portage OpeniBoot pour ce SoC ; le protocole (SPI, un ASIC Apple
  propriétaire, pas une puce du commerce) n'est connu que de la génération précédente
  ([reconnaissance](../research/2026-10-15-touch-wifi-nand-recon.md)).

## iPhone 4, A4 (S5L8930) : le deuxième banc (SOURCE, pas essayé)

- Même famille de CPU, donc le code au niveau du CPU devrait se porter avec peu de changements (**HYPOTHESIS**).
- Exploit : notre recherche a trouvé que `ipwndfu` aiguille **CPID 8930 vers SHAtter**, pas limera1n
  (**SOURCE**, [correction](../research/2026-10-14-pre-a5-limera1n.md)). D'anciens textes citent aussi
  limera1n pour les appareils A4 : les sources divergent, `ipwndfu -p` tranche. Hôte direct dans les
  deux cas, sans carte auxiliaire.
- Adresses lues dans le `plat-a4` d'OpeniBoot (**SOURCE**, aucune vérifiée sur un téléphone) : framebuffer
  `0x4F700000` ou `0x5F700000` (deux variantes dans son `clcd.h`), GPIO `0xBFA00000`, quatre contrôleurs
  d'interruptions VIC0 à VIC3 en `0xBF200000`, `0xBF210000`, `0xBF220000`, `0xBF230000`.
  L'écran est Retina, 640 x 960 (**HYPOTHESIS**, à confirmer) : la police de la console doit s'adapter.
- Des appareils sont en main (plusieurs iPhone 4 pour pièces), donc un test coûte peu. **Critère de
  réussite :** une console à l'écran en écrivant seulement `soc/s5l8930.h`, sans rien changer d'autre. Si
  ça marche, le découpage du code par puce est validé.
- `env_set`, la valeur de `run` et tous les offsets de l'iBSS sont propres à l'image 3GS et sont à
  retrouver dans l'iBSS de l'A4 (**HYPOTHESIS**).

## iPhone 4S et iPad 2, A5 (S5L8940) : le premier démarrage différent (SOURCE ; un iPad 2 identifié, boot pas essayé)

- Le boot demande un **Pico exécutant checkm8-a5** (RP2040 avec un UF2 par SoC et un câble OTG en Y ;
  RP2350 ne marche pas). Carte des SoC : 8940 = iPhone 4S et iPad 2 (sauf iPad2,4) (**SOURCE**,
  [recherche](../research/2026-10-13-a5-a6-32bit.md)).
- Matériel disponible : un **iPad 2** (modèle A1395, `iPad2,1`, carte K93AP), sous iOS 9.3.5, déverrouillé par son propriétaire. Son identité a été lue
  en USB avec `ideviceinfo`, une requête en lecture seule au service de l'appareil lui-même : ChipID 35136, soit 0x8940, le S5L8940
  (**DEVICE** : une lecture d'identité, pas l'exécution d'un payload, donc pas de dossier de preuve ; à reproduire avec `ideviceinfo -s`). La capacité
  de 32 Go et la version d'iOS viennent de la même requête. Rien n'y a été écrit et il n'a pas été mis en DFU. Le numéro de série n'est pas publié ici.
- Base : **Project Cascadia** fait tourner Linux 6.12 mainline sur l'iPad mini 1 (S5L8942), avec les deux
  cœurs, le multitouch et le Wi-Fi, démarré depuis l'hôte en RAM. Son chemin : primepwn, un iBSS patché,
  un iBEC `autogo`, un chargeur d'étape, puis le noyau (**SOURCE**). Aucun portage n'existe pour le 4S.
- Le premier travail borné est de passer Cascadia du S5L8942 au S5L8940 (**SOURCE**, note de recherche).
- Recouvrement avec le 3GS : 32 bits et captifs tous deux, mais l'exploit, la carte auxiliaire et la chaîne
  de chargement diffèrent. Notre méthode de payload nu peut se reporter ; la commande de boot peut-être pas
  (**HYPOTHESIS**).

## A6 (iPhone 5, 5c) : voisine de l'A5 (SOURCE)

- checkm8 tourne nativement dans `ipwndfu` en USB simple (moins fiable depuis un hôte Linux). Outils
  cités : ipwnder32, ipwnder_lite, gaster, primepwn. L'iPad 4 (A6X) est la feuille de route annoncée de
  Cascadia (**SOURCE**). Rien de connu pour l'iPhone 5 ou 5c.

## A7 à A11 (iPhone 5s à X) : une autre piste (SOURCE)

- 64 bits. Les device trees de tous sont dans Linux mainline depuis la 6.13 (framebuffer de boot, boutons,
  luminosité, et NVMe interne sur A11). Pas de vrai pilote d'écran, ni Wi-Fi, audio, caméra, GPU, tactile.
- Boot : checkm8, puis pongoOS, puis m1n1, puis Linux. En amont : Hoolock Linux ; aussi postmarketOS
  `apple-idevice`. Captif.
- La règle de Trognon ici : **contribuer en amont, ne pas forker**
  ([recherche](../research/2026-10-13-a7-a11-mainline.md)). Notre code 32 bits ne s'applique pas ; ce qui se
  reporte, c'est la méthode de documentation et l'idée de preuve par appareil.

## Ordre de travail

1. 3GS : le timer, puis les boutons.
2. iPhone 4 : prouver le découpage par puce avec un deuxième banc.
3. iPhone 4S et iPad 2 : le Pico, puis le chemin de Cascadia sur S5L8940. L'iPad 2 est déjà identifié (lecture seule) ; son entrée en DFU est la première étape.
4. Le reste suit les projets amont déjà actifs.

## Un dépôt, deux pistes

Décision (2026-10-15) : Trognon reste **un seul dépôt avec deux pistes nommées**, par technologie :
**A, bare metal** (3GS et iPhone 4) et **B, OS sur A5 et suivants** (4S et suite). La piste B n'a pas
encore de code propre : un dépôt séparé serait vide et couperait un public déjà petit. **Règle de scission :**
le jour où la piste B contient du code qui n'a plus rien à voir avec `hal/` (par exemple le chargeur A5 pour le
Pico), elle passe dans son propre dépôt, avec un noyau commun dans un dépôt `-shared` si les deux en ont encore
besoin. Le 3GS et l'iPhone 4 restent ensemble : ils partagent une méthode et un cœur CPU, pas leurs adresses,
donc passer de l'un à l'autre est un deuxième en-tête `soc/`, pas un deuxième projet.
