# 6. Support par puce

Trognon couvre les iPhone 32 bits **d'avant l'A5** : l'iPhone 3GS d'abord, puis l'iPhone 4. Il se construit
**de bas en haut à partir du 3GS**. Cette page dit, pour chaque puce, ce qu'on sait, comment elle démarre, et
ce qui se reporte depuis la voisine. Étiquettes de preuve comme dans l'[index](README.md). Nous n'avons touché
que le 3GS : tout ce qui concerne l'iPhone 4 est **SOURCE** (lu dans nos notes de recherche) ou **HYPOTHESIS**.

## Où passe la ligne

La ligne est l'A5, parce que c'est là qu'un amont commence.

| | **Dans Trognon : avant A5** (3GS, iPhone 4) | **Au-delà de l'A4** (A5 et après) |
|---|---|---|
| Exploit de boot | limera1n (3GS), SHAtter (A4, selon ipwndfu) : bugs bootrom de 2010, **avant** checkm8 (**SOURCE**) | checkm8 (2019), A5 à A11 (**SOURCE**) |
| Carte auxiliaire | aucune, hôte direct | l'A5 demande un Pico (**SOURCE**) ; les suivantes non |
| Travail d'OS existant | rien sur quoi construire aujourd'hui : OpeniBoot, dormant (**SOURCE**) | Project Cascadia (A5), Hoolock, arbre Asahi, postmarketOS (**SOURCE**) |
| Ce qu'on y fait | **bare metal** : nos propres payloads de boot, pilotes et [Mods](04-mods.md) | on suit les projets amont et on y contribue ; voir [Au-delà de l'A4](#au-dela-de-la4-a5-et-apres) |

Aucun amont n'est actif sur le 3GS ni sur l'iPhone 4 (OpeniBoot est dormant, **SOURCE**) : tout est donc
prouvé ici d'abord. À partir de l'A5, d'autres font déjà le travail, et Trognon ne le refait pas. Ce qui passe
d'un côté à l'autre, c'est la méthode et les habitudes (niveaux de preuve, idée de la console à l'écran), pas
les adresses. Les deux téléphones de Trognon diffèrent par l'outil d'exploit (limera1n contre SHAtter), et
d'anciens textes disent limera1n pour les deux : la
[note de recherche](../research/2026-10-14-pre-a5-limera1n.md) porte la correction.


## Frise, puce par puce

Une carte par génération de puce, dans l'ordre de sortie. Couleur du bord gauche : dans Trognon ou au-delà. Fond vert : là où notre code tourne. Contour rouge : le prochain banc.

--8<-- "docs/snippets/timeline.fr.html"

## Recouvrements en un coup d'œil

| | 3GS (S5L8920) | iPhone 4 (A4, S5L8930) |
|---|---|---|
| Notre statut | **DEVICE** : le code tourne | pas essayé |
| Cœur CPU | Cortex-A8 r2p1 (**DEVICE**, `MIDR`) | classe Cortex-A8 (**HYPOTHESIS**, lire `MIDR` d'abord) |
| Jeu d'instructions | ARMv7 32 bits | ARMv7 32 bits |
| Exploit de boot | limera1n (**DEVICE**) | SHAtter selon ipwndfu (**SOURCE**) |
| Carte auxiliaire | aucune | aucune |
| Méthode de boot réutilisable ? | la référence | oui : même forme (DFU pwned, iBSS patché, run) |
| Code CPU, console d'exceptions | la référence | réutilisable (**HYPOTHESIS**) |
| Adresses, écran, iBSS | vérifiés | **à refaire** |
| Travail OS existant | OpeniBoot/iDroid, dormant (**SOURCE**) | partiel dans OpeniBoot, dormant (**SOURCE**) |

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
- **Critère de
  réussite :** une console à l'écran en écrivant seulement `soc/s5l8930.h`, sans rien changer d'autre. Si
  ça marche, le découpage du code par puce est validé.
- `env_set`, la valeur de `run` et tous les offsets de l'iBSS sont propres à l'image 3GS et sont à
  retrouver dans l'iBSS de l'A4 (**HYPOTHESIS**).

## Frères aînés de la même époque (SOURCE, pas essayés)

Le catalogue d'appareils connaît aussi l'iPhone 2G, l'iPhone 3G et les iPod touch 1G à 4G
([recherche](../research/2026-10-17-pre-a5-older-siblings.md)). Ils sont de la même époque, mais pas égaux aux deux
téléphones ci-dessus :

- Les **iPod touch 3G** et **4G** suivent le 3GS et l'iPhone 4 : mêmes exploits selon `ipwndfu` (limera1n pour le
  CPID 8922, SHAtter pour 8930). L'iPod touch 4G partage le CPID de l'A4 avec l'iPhone 4. Le CPID que rapporte un
  exemplaire de la 3G n'est pas tranché (la source donne à la fois S5L8920 et S5L8922).
- L'**iPod touch 2G** (S5L8720) démarre avec steaks4uce dans `ipwndfu`.
- L'**iPhone 2G, l'iPhone 3G et l'iPod touch 1G** (S5L8900) sont en **ARMv6**, pas en ARMv7 : notre code au niveau
  du CPU ne se reporte pas tel quel (**HYPOTHESIS**), et `ipwndfu` n'a pas de voie DFU pwned pour eux.

Aucun n'a été essayé ; aucune ligne n'est ajoutée au tableau tant qu'un exemplaire n'a pas été sondé.

## Au-delà de l'A4 : A5 et après

Trognon s'arrête avant l'A5 (décision du 2026-10-17). À partir d'ici il existe déjà un amont, et la règle est
de **contribuer là-bas, pas de forker** ([recherche](../research/2026-10-13-a7-a11-mainline.md)). Tout ce qui
suit est **SOURCE** : lu dans nos notes de recherche, pas essayé par nous.

- **A5 (iPhone 4S, iPad 2).** Le boot demande un Pico exécutant checkm8-a5 (RP2040 avec un UF2 par SoC et un
  câble OTG en Y ; RP2350 ne marche pas). Base : **Project Cascadia**, Linux 6.12 mainline sur l'iPad mini 1
  (S5L8942), avec les deux cœurs, le multitouch et le Wi-Fi. Aucun portage n'existe encore pour un téléphone
  ([recherche](../research/2026-10-13-a5-a6-32bit.md)).
- **A6 (iPhone 5, 5c).** checkm8 tourne nativement dans `ipwndfu` en USB simple. L'iPad 4 (A6X) est la feuille
  de route annoncée de Cascadia. Rien de connu pour l'iPhone 5 ou 5c.
- **A7 à A11 (iPhone 5s à X).** 64 bits, une autre piste. Les device trees de tous sont dans Linux mainline
  depuis la 6.13 ; pas encore de vrai pilote d'écran, ni Wi-Fi, audio, caméra, GPU, tactile. Boot : checkm8,
  puis pongoOS, puis m1n1, puis Linux. En amont : Hoolock Linux et postmarketOS `apple-idevice`. Notre code
  32 bits ne s'applique pas ; ce qui se reporte, c'est la méthode de documentation et l'idée de preuve par
  appareil.

Un appareil A5 (un iPad 2) a été identifié en USB, en lecture seule, le 2026-10-15 ; ce travail ne fait pas
partie de ce dépôt.

## Ordre de travail

1. 3GS : le timer, puis les boutons.
2. iPhone 4 : prouver le découpage par puce avec un deuxième banc.
3. Au-delà de l'A4 : suivre les projets amont déjà actifs.

## Décision de périmètre

Décision (2026-10-17) : Trognon couvre les iPhone 32 bits **d'avant l'A5** et reste un seul dépôt. Elle
remplace la décision du 2026-10-15 (un dépôt, deux pistes : bare metal, puis OS sur l'A5 et après). La raison :
Project Cascadia commence à l'A5, donc le 3GS et l'iPhone 4 sont les appareils sans amont actif, et refaire le
chemin de Cascadia dans un dépôt public n'apporterait rien. Le 3GS et l'iPhone 4 restent ensemble : ils
partagent une méthode et un cœur CPU, pas leurs adresses, donc passer de l'un à l'autre est un deuxième
en-tête `soc/`, pas un deuxième projet.
