# 3. Carte matérielle : ce qui est vérifié

Les adresses de registres viennent d'OpenIBoot (`plat-s5l8920`). Elles restent **SOURCE** tant
qu'elles ne sont pas lues sur le téléphone. Cette page liste ce qu'on a vérifié. Les lectures sont
en lecture seule ; chaque chargement est armé comme un data abort attendu : un registre non mappé
affiche `ABORT` au lieu de réinitialiser le téléphone.

## CPU et MMU (DEVICE)

| Élément | Valeur | Sens |
|---|---|---|
| `MIDR` | `0x412FC081` | ARM, partie `0xC08`, variante 2, révision 1 : Cortex-A8 r2p1 |
| `SCTLR` | `0x00C5187D` | M=1 (MMU), C=1 (cache D), I=1 (cache I), Z=1, A=0 (pas de contrôle d'alignement), V=0 (vecteurs en VA 0) |
| `DACR` | `0x00000003` | domaine 0 manager, autres domaines sans accès |
| Table de pages | sections de 1 Mio, toutes en domaine 0 | rien de non mappé aux index 1..0xfff |

## Contrôleur d'affichage, `CLCD_BASE = 0x85400000` (DEVICE)

| Décalage | Valeur lue | Sens |
|---|---|---|
| `+0x0` | `0x00020085` | bit 0 à 1 : actif (OpenIBoot écrit `0x20084`) |
| `+0x24` | `0x4fd00000` | adresse du framebuffer, confirmée par un remplissage visible |
| `+0x1B10` | `0x00000001` | enable |
| `+0x1B14` | varie à chaque lecture | **compteur vivant**, pas une configuration : octet bas `0xE0` stable, bits hauts qui avancent. Les bits 0-3 valent 0, comme ce qu'écrit OpenIBoot |
| `+0x1B1C` | `0x00050507` | porches verticaux (valeur stockée moins 1) |
| `+0x1B20` | `0x000b0b0f` | porches horizontaux (stockés moins 1) |
| `+0x1B24` | `0x013f01df` | 320 x 480 (0x13f, 0x1df) |

La dalle est pilotée en MIPI DSI (ligne de reset probablement GPIO `0x500`) : le contrôleur est déjà
initialisé par l'iBSS, c'est pourquoi on peut simplement dessiner. Écrire notre propre init de dalle
est un chantier bien plus gros (**HYPOTHESIS**, voir `pinot_init` d'OpenIBoot).

## GPIO et horloges (DEVICE, partiel)

| Adresse | Valeur | Statut |
|---|---|---|
| Base GPIO `0x83000000` | lisible, sans abort | mappée, les décalages `(8*port+pin)*4` répondent |
| `GPIO+0xA0` (broche `0x500`) | bit 0 = 1 | cohérent avec un reset de dalle relâché, **non prouvé** |
| `GPIO+0x00 / +0x80 / +0x320` | lus, rôle inconnu | ne pas interpréter |
| Clock gate `0xBF100128` | `0x0000000F` | lue seulement |

Valeurs lues à l'œil sur l'écran ; largeur de champ supposée à 8 chiffres hexa.

## Non lu, volontairement

`GPIOIC` (`0x39A00000`) : l'unique lecture a fait tomber la liaison USB (cause non déterminée).
À tenir pour dangereux tant que ce n'est pas compris.

## Contrôleurs d'interruptions VIC0/1/2 (DEVICE, 2026-10-15)

Relevé en lecture seule. Tous les registres ont répondu, sans abort.

| Registre | VIC0 `0xBF200000` | VIC1 `0xBF210000` | VIC2 `0xBF220000` |
|---|---|---|---|
| `STATUS` (`+0x00`) | `0` | `0` | `0` |
| `RAW` (`+0x08`) | `0` | `0` | `0` |
| `SELECT` (`+0x0C`, 0 = IRQ) | `0` | `0` | `0` |
| `ENABLE` (`+0x10`) | `0x3FFA4040` | `0` | `0` |
| `SOFTINT` (`+0x18`) | `0` | `0` | `0` |

- L'iBSS a laissé **14 sources activées dans VIC0** (bits 6, 14, 17 et 19 à 29), toutes en IRQ, aucune
  en attente, et les IRQ sont démasquées côté CPU : les interruptions sont vivantes pendant que notre code tourne.
- `SOFTINT` fonctionne : y écrire le bit 0 a fait lire 1 à `RAW` et `STATUS` et a levé une vraie IRQ, prise par
  notre gestionnaire (voir le [test d'IRQ](02-exception-console.md)). `INTENCLEAR` vide `ENABLE`, `SOFTINTCLEAR` efface
  l'interruption logicielle, relus tous deux sur l'appareil.
- Les gestionnaires de l'iBSS sont en `0x8400F9EC` (mot de vecteur IRQ `0x38`) et `0x8400FA34` (mot FIQ `0x3C`).
- Quel périphérique correspond à chaque bit activé reste inconnu.

Les registres du timer sont la partie la moins comprise d'OpenIBoot (son propre commentaire dit qu'ils sont copiés d'un SoC plus ancien).

## Bloc timer, `TIMER_BASE = 0xBF100000` (DEVICE, 2026-10-15)

| Registre | Décalage | Observé |
|---|---|---|
| Tick libre, poids faible | `+0x200` | avance ; **24,001 MHz mesurés contre l'horloge du Mac** (3 échantillons, 12,2 s, à 0,004 % de 24 MHz) |
| Tick libre, poids fort | `+0x204` | `0` (le mot de 32 bits boucle toutes les ~179 s) |
| `STATE` timer 0 | `+0x04` | `0x00669601` |
| `STATE` timer 1 | `+0x24` | `0x00000E02` |
| `STATE` timers 2 et 3 | `+0x44`, `+0x64` | `0x00000B00` chacun |
| `STATE` timer 5 | `+0xC4` | `0` (semble inutilisé) |
| `STATE`, `CFG`, `CNT`, `CNT2` timer 4 ; `STATE` timer 6 ; clock gate `0x25` | `+0xA4, +0xA0, +0xA8, +0xAC, +0xE4, +0x10C` | tous exactement `0x0000000F` ; `PRES` du timer 4 (`+0xB0`) lit `0` |

- Le tick tourne à 24 MHz. OpeniBoot dit la même chose (`TicksPerSec = PLLFrequencies[3]`, **SOURCE**) : la
  mesure est une confirmation, pas une surprise.
- Les mots `STATE` des timers 0 à 3 semblent structurés (ce n'est probablement pas qu'un bit de démarrage) ; on ne les décode pas.
- Le mur de `0x0000000F` sur les timers 4 et 6 n'est pas expliqué : il ressemble aux registres de clock gate (`0xF`
  partout), donc ces registres sont peut-être des alias ou non implémentés. Ne pas les interpréter.
- `IRQSTAT` et `IRQLATCH` (`+0x10000`, `+0xF8`) n'ont pas été lus : ils peuvent s'effacer à la lecture.
- **Une interruption de timer n'est pas prouvée.** La ligne 6 de VIC0 (la ligne timer d'OpeniBoot) peut être
  prise par notre gestionnaire, mais la source n'est pas la nôtre : avec le timer 5 jamais démarré, l'interruption est
  quand même arrivée 127 ticks (5 microsecondes) après l'activation de la ligne, parce qu'elle était déjà levée.
  `IRQSTAT` (`+0x10000`) lit `0xFFFFFF81` avant, pendant et après, quoi qu'on fasse : ce n'est pas un registre de
  statut de timer, et la séquence d'acquittement copiée d'OpeniBoot (`IRQSTAT`, `IRQLATCH`) n'efface rien. Le gestionnaire
  de timer d'OpeniBoot pour cette puce est commenté. Tenir ces deux décalages pour faux tant qu'ils ne sont pas prouvés.
- Outil de mesure : `make trace_ticks.bin` et `tools/tick_rate.py`. Il publie le compteur dans une variable
  d'environnement lue par `getenv`, donc sans photo. Deux échantillons dans deux invocations séparées : un
  processus ne peut pas garder l'appareil à travers une pause (la deuxième connexion échoue).

## NAND, `H2FMI0_BASE = 0x81200000` (DEVICE, 2026-10-15)

Lecture d'identifiant (lecture pure : reset + commande READ ID, aucune lecture de page, aucune
écriture) sur les 16 lignes de chip-select du bus 0, puis sa géométrie retrouvée dans la table
figée d'OpeniBoot.

| Élément | Valeur | Signification |
|---|---|---|
| CE0, CE1 | `98 DE 95 32 7A 54 0C 00` | Toshiba (fabricant JEDEC `0x98`) ; même identifiant sur les deux, recoupement |
| CE2 à CE15 | `00 00 00 00` | aucune puce présente |
| Géométrie (table OpeniBoot `nand_chip_info[]`, correspondance exacte) | `blocks_per_ce=0x2068`, `pages_per_block=0x80`, `bytes_per_page=0x2000`, `banks_per_ce=2` | correspondance SOURCE, pas lue sur l'appareil |
| Capacité, calculée | 8296 x 128 x 8192 x 2 banks x 2 CE = 34,8 Go bruts | cohérent avec les 32 Go de l'étiquette de cet exemplaire, marge ECC/spare habituelle retirée (~8 %) |

Bus 1 (`H2FMI1_BASE = 0x81300000`) pas encore balayé. Aucune lecture de page, aucune écriture
tentée : la même prudence que ce projet applique au NOR (`tools/boot.sh` n'appelle jamais
`flash_nor()`) s'applique ici aussi, avec une marge plus grande, puisque le NAND porte l'image
iOS de l'appareil.

## Boutons, même base GPIO que ci-dessus (DEVICE, 2026-10-15)

Trouvés par différence, pas par le nom : les noms de boutons d'OpeniBoot pour ce SoC semblent
copiés d'une génération antérieure, donc chaque bit GPIO lisible (`(8*port+pin)*4` depuis la
base ci-dessus) a été échantillonné, le bruit mesuré mains levées, puis chaque bouton demandé
à tour de rôle.

| Bouton | Broche (`port.pin`) |
|---|---|
| HOME | `16.6` |
| POWER | `16.7` |
| VOL UP | `16.0` |
| VOL DOWN | `16.1` |
| RINGER (bascule) | `14.3` |

376 broches lues, zéro bruit sur une fenêtre de 3 secondes mains levées. Un payload long (ce
balayage prend près de 3 minutes) a tué le point de contrôle USB ensuite ; un reset de port USB
côté hôte (`dev.reset()`, pas besoin de DFU) l'a réveillé — voir [preuves](08-evidence.md).

## Wi-Fi/SDIO, `SDIO0_BASE = 0x80000000` (DEVICE/HYPOTHÈSE mêlées, 2026-10-15/16)

Aucun code source OpeniBoot n'existe pour ce bloc sur le S5L8920 (contrairement au NAND). Tout
ce qui suit l'adresse elle-même vient du device tree et du kernelcache déchiffrés de cet
appareil (IPSW du propriétaire, jamais redistribués), pas du code source.

| Élément | Valeur | Statut |
|---|---|---|
| Adresse de base, taille | `0x80000000`, `0x1000` | DEVICE (propriété `reg` du device tree) |
| `compatible` | `"sdio,s5l8920x"` | DEVICE |
| `clock-gates` | `0x10` | DEVICE (device tree) ; l'activer seul n'a fait répondre aucun registre |
| `interrupts` | `0x22` | DEVICE |
| GPIO de reset (`gpio_baseband_wlan_reset`) | `16.5` | DEVICE ; lue à `0`, puis basculée bas 12 ms / haut 150 ms, relue à `1` — écriture confirmée |
| GPIO de réveil (`gpio_sdio_device_wake`) | `14.4` | DEVICE ; lue à `0` |
| Tout registre du bloc (`CTRL`, `CMD`, capacités...) | lit `0`, les écritures ne tiennent pas | DEVICE, confirmé plusieurs fois (clock-gate activé, séquence d'init complète façon SDHCI, écriture brute puis relecture) |

**Pourquoi les écritures ne tiennent pas** : un second processeur ARM embarqué (l'« IOP »
d'Apple) possède ce bloc directement et fait tourner son propre système temps réel ; le CPU
principal ne fait que poster des messages dans un canal de commande partagé. C'est l'explication
la plus probable, pas encore pleinement prouvée — voir le
[rapport sur l'architecture Wi-Fi/IOP](../research/2026-10-16-wifi-iop-architecture.md) pour la
chaîne de preuve complète, ce qui est établi contre ce qui reste hypothèse, et pourquoi le NAND
(pas le Wi-Fi) est très probablement la vraie raison d'être de ce coprocesseur.
