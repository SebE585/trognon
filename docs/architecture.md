# Architecture

Statut : brouillon, 2026-10-13. Rien n'a tourné sur un appareil réel.

## Principe : orchestrer, ne pas réécrire

Trognon n'implémente pas d'exploit. L'étape qui met un appareil en
« pwned DFU » est déjà couverte par des outils publics et maintenus
(ipwndfu, gaster, checkm8-a5 sur Raspberry Pi Pico — détails et sources dans
[`docs/research/2026-10-13-a5-a6-32bit.md`](research/2026-10-13-a5-a6-32bit.md)
et [`2026-10-13-untethered-boot.md`](research/2026-10-13-untethered-boot.md)).

Le framework C (`libtrognon` + CLI) se limite à :
1. **identifier** l'appareil branché (lecture standard de descripteurs USB,
   rien d'intrusif) ;
2. **choisir** et **lancer** le bon outil externe pour amener l'appareil en
   DFU débridé (sous-processus, pas de code d'exploitation dans ce dépôt) ;
3. une fois l'appareil en DFU débridé — état normal et documenté, pas un
   exploit — **envoyer** une charge (pongoOS, puis noyau) par le protocole
   USB DFU standard ;
4. tenir la **base des appareils** (SoC, famille, état de support) qui sert
   à toutes les couches au-dessus.

## Couches

```
┌─────────────────────────────────────────┐
│ 4. Ce que l'utilisateur construit         │  Linux, BSD, mini-OS, bare-metal
│    (hors scope Trognon : chacun le sien)  │
├─────────────────────────────────────────┤
│ 3. Charge utile de démarrage              │  pongoOS / m1n1 / noyau — upload
│    (trognon-boot)                         │  USB une fois l'appareil débridé
├─────────────────────────────────────────┤
│ 2. Orchestration de la mise en DFU        │  appelle ipwndfu / gaster /
│    (trognon-unlock)                       │  checkm8-a5, outils TIERS non
│                                            │  réimplémentés ici
├─────────────────────────────────────────┤
│ 1. Identification + base des appareils    │  USB probe (lecture seule) +
│    (libtrognon)                           │  table statique SoC ↔ modèle
└─────────────────────────────────────────┘
```

## Pourquoi le C

- C'est le langage des outils existants qu'on orchestre (libusb, pongoOS,
  m1n1, le noyau Linux lui-même).
- Pas de runtime à embarquer sur un pont USB minimal (microcontrôleur du
  futur dongle).
- Portable BSD ↔ Linux ↔ macOS pour l'outil hôte.

## Arborescence prévue

```
include/trognon/
  device.h     — struct trognon_device, table SoC ↔ modèle
  usb.h        — probe USB en lecture seule (libusb-1.0)
  chain.h      — enum des étapes de la chaîne de démarrage, état
  log.h        — logging commun
src/
  device_db.c  — table statique (A5 à A11), read-only
  usb_probe.c  — énumération + identification (pas d'écriture)
  chain.c      — orchestration : lance les outils tiers en sous-processus
  cli.c        — `trognon` : point d'entrée
tools/
  wrappers vers les outils tiers (installés séparément, jamais vendorés)
tests/
  tests sur device_db et le parsing des descripteurs USB (pas de matériel requis)
```

## Dépendances externes, jamais vendorées dans ce dépôt

| Outil | Rôle | Dépôt |
|---|---|---|
| libusb-1.0 | accès USB bas niveau | système (`brew`/`apt`) |
| ipwndfu / gaster | checkm8 A7–A11 | upstream, appelé en sous-processus |
| checkm8-a5 | checkm8 A5 via Pico/Arduino | upstream, firmware flashé à part |
| pongoOS | micro-noyau de staging | upstream |
| m1n1 (branche idevice, Hoolock) | bootloader intermédiaire | upstream |

## État actuel (2026-10-14)

Couche 1 (`libtrognon`) et une première tranche de la couche 2 existent :
`trognon unlock` identifie l'appareil branché, décide de l'action suivante
(mettre en DFU / lancer un outil / déjà débridé) et **affiche** l'outil
recommandé — il ne lance encore aucun sous-processus. La logique de
planification (`src/chain.c`, `trognon_chain_plan()`) est pure et testable
sans matériel (`tests/test_chain.c`).

L'exécution réelle (fork/exec vers `ipwndfu`/`gaster`/Legacy iOS Kit, jamais
`system()`) est volontairement reportée : la syntaxe exacte de chaque outil
n'a pas encore été vérifiée sur un appareil réel, et fabriquer des arguments
de commande non vérifiés serait pire que de ne pas automatiser cette étape.

## Prochaine étape concrète

Installer et vérifier la syntaxe réelle d'au moins un outil (`ipwndfu` ou
Legacy iOS Kit) contre le 3GS déjà en DFU host-direct, puis ajouter
l'exécution dans `src/chain.c` derrière un flag explicite (pas par défaut).
