# 3. Carte matérielle : ce qui est vérifié

Les adresses de registres viennent d'OpenIBoot (`plat-s5l8920`). Elles restent **SOURCE** tant
qu'elles ne sont pas lues sur le téléphone. Cette page liste ce qu'on a vérifié. Les lectures sont
en lecture seule ; chaque chargement est armé comme un data abort attendu : un registre non mappé
affiche `ABORT` au lieu de réinitialiser le téléphone.

## CPU et MMU (DEVICE)

| Élément | Valeur | Sens |
|---|---|---|
| `MIDR` | `0x412FC0F1` | ARM, partie `0xC08` : Cortex-A8 |
| `SCTLR` | `0x00C50078` | MMU + caches actifs, contrôle d'alignement coupé |
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

## Suite sur cette carte

VIC0/1/2 (`0xBF200000/10000/20000`, **SOURCE**) : relevé du statut, de l'activation, de la sélection
et du masque de priorité, puis une interruption déclenchée par logiciel. Les registres du timer sont
la partie la moins comprise d'OpenIBoot (son propre commentaire dit qu'ils sont copiés d'un SoC plus ancien).
