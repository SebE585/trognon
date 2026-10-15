# Architecture

Statut : brouillon, mis à jour 2026-10-14. Premier succès réel le
2026-10-14 : un iPhone 3GS (S5L8920) a été débridé en DFU via `ipwndfu -p`
(limera1n), confirmé par `trognon probe`. Voir
[`2026-10-14-pre-a5-limera1n.md`](research/2026-10-14-pre-a5-limera1n.md).
Même jour, plus tard : OpeniBoot compilé pour cette cible et lu ligne par
ligne (section « S5L8920 (iPhone 3GS) » ci-dessous). Premier contrat
d'entrée défini, pas encore vérifié sur le matériel.

## Principe : orchestrer, ne pas réécrire

Trognon n'implémente pas d'exploit. L'étape qui met un appareil en
« pwned DFU » est déjà couverte par des outils publics et maintenus
(ipwndfu, gaster, checkm8-a5 sur Raspberry Pi Pico). Détails et sources dans
[`docs/research/2026-10-13-a5-a6-32bit.md`](research/2026-10-13-a5-a6-32bit.md)
et [`2026-10-13-untethered-boot.md`](research/2026-10-13-untethered-boot.md).

Le framework C (`libtrognon` + CLI) se limite à :
1. **identifier** l'appareil branché (lecture standard de descripteurs USB,
   rien d'intrusif) ;
2. **choisir** et **lancer** le bon outil externe pour amener l'appareil en
   DFU débridé (sous-processus, pas de code d'exploitation dans ce dépôt) ;
3. une fois l'appareil en DFU débridé (état normal et documenté, pas un
   exploit), **envoyer** une charge (pongoOS, puis noyau) par le protocole
   USB DFU standard ;
4. tenir la **base des appareils** (SoC, famille, état de support) qui sert
   à toutes les couches au-dessus.

## Couches

```
+-----------------------------------------+
| 4. Ce que l'utilisateur construit       |  Linux, BSD, mini-OS, bare-metal
|    (hors scope Trognon : chacun le sien)|
+-----------------------------------------+
| 3. Charge utile de démarrage            |  pongoOS / m1n1 / noyau, upload
|    (trognon-boot)                       |  USB une fois l'appareil débridé
+-----------------------------------------+
| 2. Orchestration de la mise en DFU      |  appelle ipwndfu / gaster /
|    (trognon-unlock)                     |  checkm8-a5, outils TIERS non
|                                         |  réimplémentés ici
+-----------------------------------------+
| 1. Identification + base des appareils  |  USB probe (lecture seule) +
|    (libtrognon)                         |  table statique SoC / modèle
+-----------------------------------------+
```

## Pourquoi le C

- C'est le langage des outils existants qu'on orchestre (libusb, pongoOS,
  m1n1, le noyau Linux lui-même).
- Pas de runtime à embarquer sur un pont USB minimal (microcontrôleur du
  futur dongle).
- Portable entre BSD, Linux et macOS pour l'outil hôte.

## Arborescence prévue

```
include/trognon/
  device.h     : struct trognon_device, table SoC / modèle
  usb.h        : probe USB en lecture seule (libusb-1.0)
  chain.h      : enum des étapes de la chaîne de démarrage, état
  log.h        : logging commun
src/
  device_db.c  : table statique (A5 à A11), read-only
  usb_probe.c  : énumération + identification (pas d'écriture)
  chain.c      : orchestration, lance les outils tiers en sous-processus
  cli.c        : `trognon`, point d'entrée
tools/
  wrappers vers les outils tiers (installés séparément, jamais vendorés)
tests/
  tests sur device_db et le parsing des descripteurs USB (pas de matériel requis)
```

## Dépendances externes, jamais vendorées dans ce dépôt

| Outil | Rôle | Dépôt |
|---|---|---|
| libusb-1.0 | accès USB bas niveau | système (`brew`/`apt`) |
| ipwndfu / gaster | checkm8 A7 à A11 | upstream, appelé en sous-processus |
| checkm8-a5 | checkm8 A5 via Pico/Arduino | upstream, firmware flashé à part |
| pongoOS | micro-noyau de staging | upstream |
| m1n1 (branche idevice, Hoolock) | bootloader intermédiaire | upstream |

## État actuel (2026-10-14)

Couche 1 (`libtrognon`) et une première tranche de la couche 2 existent :
`trognon unlock` identifie l'appareil branché, décide de l'action suivante
(mettre en DFU / lancer un outil / déjà débridé) et **affiche** l'outil
recommandé. Il ne lance encore aucun sous-processus. La logique de
planification (`src/chain.c`, `trognon_chain_plan()`) est pure et testable
sans matériel (`tests/test_chain.c`).

L'exécution réelle (fork/exec vers `ipwndfu`/`gaster`/Legacy iOS Kit, jamais
`system()`) est volontairement reportée : la syntaxe exacte de chaque outil
n'a pas encore été vérifiée sur un appareil réel, et fabriquer des arguments
de commande non vérifiés serait pire que de ne pas automatiser cette étape.

## Prochaine étape concrète

`pongoOS` ne couvre pas le S5L8920/A4 (32-bit, pré-A7). Pour cette
famille, la couche 3 réaliste n'est pas un mini-noyau mais le chargement
d'un **iBSS patché**, déjà implémenté côté outil orchestré
(`dfuexec.py`'s `boot_ibss()` dans ipwndfu, pas de code à écrire ici).
Fait le 2026-10-14 sur le 3GS : iBSS envoyé avec succès, appareil atteint
le **Recovery mode**, point le plus avancé de la chaîne de démarrage
testé à ce jour. Voir
[`2026-10-14-pre-a5-limera1n.md`](research/2026-10-14-pre-a5-limera1n.md).

Pour A7 à A11, l'étape reste d'installer et vérifier la syntaxe réelle
d'`ipwndfu`/`gaster` puis d'ajouter l'exécution dans `src/chain.c` derrière
un flag explicite (pas par défaut).

## S5L8920 (iPhone 3GS) : ce qu'on a appris d'OpeniBoot

Décision du 2026-10-14 (voir
[`2026-10-14-microkernel-3gs.md`](research/2026-10-14-microkernel-3gs.md)) :
le 3GS devient une cible réelle de micro noyau. Premier jalon : cloner et
compiler OpeniBoot/iDroid (`github.com/iDroid-Project/openiBoot`, mort
depuis 2022, révision `866562f`). Il a réussi : `iphone_3gs_openiboot`
compile et lie (ELF ARM 293 816 octets). Ce qui suit vient d'une lecture
ligne par ligne du code source compilé pour cette cible
(`plat-s5l8920/`, `arch-arm/entry.sx`, `init.c`, `openiboot.c`), pas d'une
inférence par analogie. Chaque fait cite le fichier qui le prouve. **Pas
encore testé sur l'appareil réel.**

### Séquence de démarrage

1. **`_start` (`arch-arm/entry.sx`), mode ARM obligatoire.** Table de
   vecteurs d'exception classique ARM (reset, undefined, swi, prefetch
   abort, data abort, reserved, IRQ, FIQ) à l'adresse de chargement.
2. **`ArmReset` se relocalise lui-même si besoin**, avant tout autre
   setup : il calcule son adresse de chargement réelle à partir du PC
   courant, la compare à son adresse de lien (`OpenIBootLoad = 0x0` en
   config normale, `plat-s5l8920/includes/hardware/s5l8920.h`), et si
   elles diffèrent, se recopie mot par mot vers `0x0`, vide les caches,
   puis saute à l'adresse recopiée. Conséquence pour nous : l'image
   n'a pas besoin d'être placée exactement à `0x0` par le chargeur,
   OpeniBoot s'y relocalise tout seul.
   Avant même cette relocalisation,
   il réécrit un registre de remap MIU fixe du SoC (`0xBF100000` pour le
   S5L8920) pour que l'adresse physique `0x0` pointe vers de la vraie RAM
   et pas vers de la ROM ou un périphérique. Cette écriture utilise une
   adresse absolue du SoC, donc elle marche quel que soit l'endroit d'où
   le code tourne au moment de l'exécuter.
3. **Zéro le `.bss`**, puis initialise une pile séparée pour **chaque**
   mode CPU (IRQ, FIQ, Abort, Undefined, Supervisor) avant d'appeler
   `OpenIBootStart()` (code Thumb) en mode Supervisor.
4. **`OpenIBootStart()` (`openiboot.c`) ne préserve rien de l'état
   précédent, il réinitialise tout lui-même** : `platform_init()` puis
   `init_boot_modules()` (modules enregistrés via une section de linker
   dédiée, `init.c`, même mécanisme que les initcalls Linux) puis la
   console puis `tasks_run()` (boucle infinie, scheduler coopératif par
   `task_yield()`, jamais de retour).
5. **`platform_init()` (`plat-s5l8920/s5l8920.c:22`), ordre figé et
   commenté par les auteurs eux-mêmes** : `arm_setup()`, `mmu_setup()`,
   `tasks_setup()`, `miu_setup()`, `power_setup()`, `clock_setup()`,
   `interrupt_setup()`, `gpio_setup()`, `timer_setup()`,
   `event_setup()`, `dma_setup()`, `uart_setup()`, `i2c_setup()`,
   `spi_setup()`, `aes_setup()`, dans cet ordre. Les commentaires du fichier source
   ("Basic prerequisites for everything else", "Need interrupts for
   everything afterwards") donnent les dépendances d'ordre.

### Point important pour notre contrat d'entrée : OpeniBoot ne préserve PAS l'état du chargeur précédent

La recommandation de l'étude de faisabilité du 2026-10-14
(« préserver l'état matériel laissé par l'iBSS/Recovery, ne pas couper la
MMU ni reprogrammer PLL/mémoire à l'aveugle ») est une prudence générale
raisonnable pour *notre* premier code, mais **OpeniBoot lui-même ne fait
pas ça** : `arm_setup()` (`arch-arm/arm.c`) désactive explicitement les
caches instructions/données avant de les reconfigurer et de les
réactiver, remappe le port périphérique, active le VFP ; `mmu_setup()`
(`plat-s5l8920/mmu.c`) construit une page table d'identité neuve
(sections de 1 Mo) et active la MMU sans vérifier ce qui existait avant.
Les auteurs documentent mal une partie de ça : `mmu.c:50-52`
contient `// I have absolutely no clue what this does, but
Apple does it, so hey, it must work, right?` avant un accès à un registre
coprocesseur non documenté (`GetC9C012`/`SetC9C012`). Cela confirme la mise en
garde de l'étude du 2026-10-14 : « source historique, pas une spec
constructeur ».

### Découpage mémoire (S5L8920, `plat-s5l8920/includes/hardware/s5l8920.h`)

| Zone | Valeur | Rôle |
|---|---|---|
| `OpenIBootLoad` | `0x00000000` | Adresse de lien ARM (voir relocalisation ci-dessus) |
| `RAMStart` à `RAMEnd` | `0x40000000` à `0x50000000` (256 Mo) | RAM physique, mappée cacheable+bufferable |
| `HiMemStart` à `HiMemEnd` | `0xC0000000` à `0xD0000000` (1 Go) | Alias haut de la RAM, non cacheable |
| `PageTable` | `RAMEnd - 0x8000` | Table de pages ARM (sections 1 Mo), tout en haut de la RAM |
| `HeapStart` | `0x52000000` | Début du tas (`malloc`/`memalign`, `dlmalloc`, `includes/malloc-2.8.3.h`) |
| `AMC0Start` à `AMC0End` | `0x84800000` + 8 Mo | Fenêtre mémoire d'un périphérique (contrôleur mémoire ?) |

La MMU mappe tout l'espace 32 bits en section de 1 Mo par défaut
(non cacheable), sauf la RAM physique (cacheable+bufferable) et le haut
de mémoire : une MMU « identité » pure, pas de pagination fine.

### Horloge, interruptions, GPIO : registres bruts, peu commentés

- **`clock_setup()`** (`plat-s5l8920/clock.c`) programme 3 PLL
  (`clock_setup_pll`, diviseurs/multiplicateurs codés en dur pour ce SoC)
  puis 25 horloges périphériques dynamiques depuis une table de
  constantes (`clock_setup_table`). Aucun commentaire n'explique quelle
  entrée correspond à quel périphérique, il n'y a que des indices numériques.
- **`interrupt_setup()`** (`plat-s5l8920/interrupt.c`) configure 3
  contrôleurs d'interruption (VIC0/VIC1/VIC2, style ARM PL190 étendu) :
  toutes les IRQ désactivées puis réassociées à l'index de leur table
  (pas de vecteurs directs), tout en mode IRQ (pas FIQ). Le routage réel
  entre IRQ, VIC et handler est fait à l'exécution dans `arch-arm/entry.sx`
  (`HandleIRQ`), pas en C.
- **`timer_setup()`** (`plat-s5l8920/timer.c:41-60`) écrit des valeurs
  magiques directement dans des adresses littérales (`0xBF101218`,
  `0xBF10121C`, ...) avec le commentaire `// Let's just assume this is
  timer setup... Why? It sets TicksPerSec, 2G/IPHONE_3G timer_setup did
  that, too.` Autrement dit : copié d'un SoC plus ancien, jamais
  pleinement compris ni documenté par les auteurs.
- **`power_setup()`** (`plat-s5l8920/power.c`) et **`miu_setup()`**
  (`plat-s5l8920/miu.c`) sont quasi vides sur ce SoC. Le commentaire dit
  « S5L8920 no longer has power_ctrl, just lots of clock
  gates » : le contrôle de puissance passe par `clock_gate_switch()`
  dans `clock.c`, pas par un module `power` séparé.

### Affichage : la piste la plus solide, confirmée avec de vraies données matérielles

`plat-s5l8920/clcd.c` (533 lignes) contient une init d'écran complète et
spécifique au 3GS, **pas un placeholder** :

- Panneau piloté en **MIPI DSI** (`mipi_dsim_init`/`mipi_dsim_write_data`,
  `plat-s5l8920/mipi_dsim.c`), pas un simple framebuffer mémoire : il faut
  parler au contrôleur DSI, pas juste écrire des pixels.
- Table `LCDInfoTable` (`clcd.c:48-50`) contient **une seule entrée**,
  nommée `"n88"` (le nom de code interne Apple du 3GS, cohérent avec
  `n88ap-iBEC-4.3.5.img3`, déjà utilisé dans nos envois iBEC), avec
  résolution **320×480**, ce qui correspond exactement à l'écran 3,5″ du
  3GS : donnée matérielle réelle, pas générique.
- Framebuffer alloué dynamiquement (`malloc`/`memalign`) après que la MMU
  et le tas soient up, en RGB888 32 bpp par défaut (RGB565 aussi
  supporté, `hline_rgb565`/`vline_rgb565` existent) ; `hline`/`vline`
  déjà implémentés comme primitives de dessin, ce que
  l'architecture minimale proposée par l'étude du 2026-10-14 (« texte et
  rectangles ») demande.
- **Tables de gamma** câblées en dur par identifiant de panneau
  (`PinotGammaTables`, 5 entrées avec masque/valeur d'ID) : le code lit
  l'ID du panneau physique via DSI (`mipi_dsim_read_write`) et
  choisit la bonne table. Ce mécanisme est spécifique à cette génération
  d'écran Apple, pas un standard MIPI générique.
- Init bloquante avec délais (`task_sleep`, `udelay`) calés sur le
  séquencement du contrôleur (`pinot_init()`) : éteindre/allumer une
  ligne GPIO (`0x500`, probablement reset du panneau), attendre,
  initialiser le DSI, attendre encore, lire l'ID, écrire les commandes
  d'allumage (`0x11` = sleep out, `0x29` = display on, commandes MIPI DCS
  standard, documentées par la norme).

L'étude du 2026-10-14 avait donc raison sur ce point précis : l'écran
a une base de code réelle et spécifique pour ce SoC, contrairement au
tactile où rien d'équivalent n'existe dans cette arborescence
(confirmé : aucun fichier `multitouch-*` n'est listé dans
`plat-s5l8920/SConscript`, contrairement à `plat-s5l8900/`).

### Console : USB, pas UART série

`acm/acm.c` implémente une console sur **USB** (bulk, endpoints dédiés
envoi/réception, tampon 1 Ko), pas sur l'UART physique du connecteur.
C'est cohérent avec la trace 2015 citée dans l'étude (« console ACM » sur un
3GS réel). `uart_setup()` existe (`plat-s5l8920/SConscript` liste
`uart.c`) mais rien dans `s5l8920.c`/`acm.c` ne montre qu'il sert de
console par défaut. À vérifier si on en a besoin un jour (l'UART du 3GS
n'est de toute façon pas exposé sur un connecteur accessible sans
dessoudage, contrairement à certains dev boards).

### Inconnues restantes

- Rien de ce qui précède n'a tourné sur l'appareil réel : lecture de
  source uniquement, pas de test matériel aujourd'hui.
- Pas de preuve que notre iBSS patché (Trognon, via `ipwndfu`) puisse
  charger une image OpeniBoot sans adaptation. Voir « contrat d'entrée »
  ci-dessous, à vérifier sur le matériel avant tout envoi réel.
- `0x3400` octets de table de pages avant `RAMEnd` : on n'a pas vérifié
  que `boot_ibss()`/Recovery (notre point d'entrée actuel) laisse cette
  zone libre.
- Le registre coprocesseur touché par `initialize_pagetable()`
  (`GetC9C012`/`SetC9C012`) n'est documenté nulle part dans ce dépôt ni
  trouvé dans cette session. Risque si on modifie ce code sans
  comprendre ce bit.

## Contrat d'entrée : notre iBSS patché, puis image chargée

Défini à partir de la lecture ci-dessus, **pas encore vérifié contre le
matériel**. C'est ce que `trognon-boot` doit garantir avant de sauter
dans une image (OpeniBoot ou notre propre code), et ce qu'une image doit
pouvoir supposer en entrant.

| Point | Valeur / exigence | Preuve |
|---|---|---|
| Mode CPU à l'entrée | **ARM (A32)**, pas Thumb | `_start` est assemblé `.code 32` (`entry.sx:25`) ; un saut en Thumb (bit 0 de l'adresse à 1) exécuterait du Thumb sur du code ARM et planterait immédiatement |
| État des registres à l'entrée | **Seul PC doit être valide.** R0-R3 sont écrasés dès les premières instructions ; pas de pile requise avant la fin de la relocalisation/zéro-BSS | `entry.sx:43-139` n'utilise que PC et R0-R3 avant d'initialiser SP |
| Adresse de chargement | **N'importe où en RAM accessible** : l'image se relocalise elle-même vers son adresse de lien (`0x0`) si besoin | `entry.sx:40-129` (bloc `ArmReset_relocate`) |
| État de la MMU/caches à l'entrée | **Indifférent** : l'image réinitialise tout elle-même (`arm_setup()`, `mmu_setup()`), ne suppose ni ne préserve un état antérieur | `arch-arm/arm.c:6-50`, `plat-s5l8920/mmu.c:19-38` |
| État des interruptions à l'entrée | **Indifférent** : `interrupt_setup()` désactive tout avant de reconfigurer | `plat-s5l8920/interrupt.c:7-35` |
| Zone RAM à ne pas écraser avant le saut | Au minimum l'image elle-même pendant sa relocalisation (jusqu'à `OpenIBootEnd - OpenIBootLoad` octets à partir de `0x0`) **et** `RAMEnd - 0x8000` (table de pages, 32 Ko) | `entry.sx:95-96` ; `s5l8920.h:31` |
| Registre à réécrire avant tout accès mémoire | Remap MIU (`0xBF100000`, SoC S5L8920), sinon l'adresse physique `0x0` ne pointe pas vers de la RAM | `entry.sx:84-93` |

**Reste à faire** : vérifier que `boot_ibss()`
d'ipwndfu (notre point d'entrée actuel, qui amène en Recovery, pas
directement à l'exécution d'une image tierce) peut charger et
sauter dans une image construite avec ce contrat. Recovery est un
protocole d'échange de commandes (`send_data`/`send_command('go')`), pas
un simple « jump to address », donc le passage de relais reste à
construire et tester, pas supposé.

### Correction, 2026-10-14 (soir) : une voie d'exécution non signée existe déjà, `run` et non `go`

Deux erreurs dans la première version de cette section (ci-dessus,
gardée barrée pour traçabilité plutôt que supprimée) :

1. **Le patch de `boot_ibss()` n'est pas limité au chemin DFU.** La
   même fonction `flash_nor()` de `dfuexec.py` le prouve en code : elle
   appelle `boot_ibss()` (qui mène en Recovery), puis **depuis Recovery**,
   `recovery.acquire_device()` + `recovery.send_data(device, payload)` +
   `recovery.send_command(device, 'run')`, et ça marche (c'est le
   mécanisme de son flash NOR, pas une démo). `run` est exactement
   la chaîne que `boot_ibss()` a substituée à `reset` dans la table de
   commandes de l'iBSS. Cette table de commandes est la même qu'on
   atteigne la commande par le protocole DFU ou par le protocole
   Recovery (même parseur de commandes iBoot, exposé par les deux
   « modes » USB). Le patch, posé une seule fois pendant la
   transition DFU vers Recovery, reste donc actif *après* qu'on soit en Recovery.
   Il n'y a donc **pas besoin de patcher `go`** : la voie non signée
   existe déjà, il suffit de l'utiliser (`run`, pas `go`).
2. **`0x41000001` n'est pas `0x84000000 + 1`.** Erreur de ma part :
   `0x84000000` (`LOAD_ADDRESS` dans `dfuexec.py`) est l'adresse du
   tampon utilisé par le mécanisme `exec` du shellcode SecureROM/DFU
   (`execute()`, avant même l'envoi de l'iBSS), un contexte différent.
   `0x41000001` pointe dans la fenêtre RAM du S5L8920
   (`RAMStart = 0x40000000`, `s5l8920.h`), à `RAMStart + 0x01000000`, bit
   0 posé (Thumb). C'est très probablement le tampon de réception fixe
   où `recovery.send_data()` dépose les octets reçus : ce transfert
   (`recovery.py:41-48`) est un bulk USB simple, sans adresse explicite,
   et c'est l'iBSS lui-même qui décide où atterrit la donnée, ce qui est
   cohérent avec cette hypothèse. **Pas encore confirmé sur le binaire
   réel** (pas de désassemblage fait), mais cohérent avec tout ce qu'on
   a lu.

**Conséquence : pas de chantier de rétro-ingénierie à lancer.** Pas de
patch à trouver dans `go`, pas de désassemblage à faire pour commencer.
Le prochain jalon (repris de la pré-étude du soir, les deux
réponses convergent) : charger un tout petit programme par
`recovery.send_data()` puis `recovery.send_command(device, 'run')`,
et observer une preuve propre à ce programme après coup (nouvelle
énumération USB, ou tout autre comportement observable). **L'absence
d'erreur USB sur `run`/`go` ne prouve rien par elle-même**, comme pour
l'iBEC envoyé plus tôt.

**Note de méthode :** le déchiffrement de l'iBSS (`image3.py`'s
`getDecryptedPayload()`, `~/tools/ipwndfu/image3.py:58`) passe par
`dfuexec.PwnedDFUDevice()`, c'est-à-dire par le moteur AES **du SoC
lui-même, via la clé GID gravée dans la puce**, pas par une clé extraite ou
connue à l'avance. **Il n'existe donc pas de corps d'iBSS déchiffré
qu'on puisse désassembler sans l'appareil branché et pwnee à ce
moment-là** (propriété de sécurité de la puce, pas une limite de
nos outils). Fait avec le 3GS branché et repwnee (`ipwndfu -p`,
confirmé `PWND:[limera1n]`, 2e tentative, même flakiness documentée
plus tôt) ce même soir.

### Confirmé par désassemblage réel (binaire déchiffré depuis l'appareil, 2026-10-14 nuit)

Déchiffrement : `image3.Image3(data).newImage3(decrypted=True)` sur
`n88ap-iBSS-4.3.5.img3` avec l'appareil pwned branché, ce qui donne 108 800 octets
(même taille que celle déjà loggée). Désassemblage Thumb (`capstone`).

- **Base de chargement/exécution de l'iBSS confirmée : `0x84000000`.**
  Désassemblage propre et cohérent (prologues de fonction valides,
  `bl` vers des adresses plausibles du même binaire) à partir de cette
  hypothèse : ce n'est pas une supposition, c'est une vérification positive.
- **Table de commandes localisée et son format décodé** : chaque
  entrée fait 16 octets, `[flags/inconnu (0 pour `reset`), ptr_nom,
  ptr_handler, ptr_aide]`. L'entrée de `reset` est exactement à
  l'offset `0x17650` (corps, après les 64 octets d'en-tête IMG3) :
  `ptr_nom = 0x84014954` pointe vers la chaîne `"reset\0"` à l'offset `0x14954`,
  `ptr_handler = 0x84000cdd` (handler original, Thumb). **Ces deux
  offsets correspondent exactement aux deux patchs documentés dans
  `dfuexec.py`** (`0x14954` pour le nom, `0x17654` pour le pointeur de
  handler, à 4 octets du nom dans cette même entrée). Les patchs de
  `boot_ibss()` visent donc les deux champs qu'on vient de localiser par lecture
  de la table elle-même, ce qui exclut la coïncidence.
- **L'entrée `reboot` suit immédiatement celle de `reset` dans la même
  table**, et partage le **même handler** (`0x84000cdd`) : `reset` et
  `reboot` sont deux noms pour la même fonction dans l'iBSS Apple
  d'origine.
- **L'entrée `go` existe séparément**, à l'offset `0x17670` :
  `ptr_nom` pointe vers `"go\0"`, `ptr_handler = 0x84000d7d` (handler différent de
  celui de `reset`/`reboot`, **non touché** par le patch de
  `boot_ibss()`, ce qui confirme qu'on n'a pas besoin d'y toucher).
- **La preuve qui ferme la boucle** : désassemblage du handler de `go`
  (`0x84000d7c`). Il charge `r0 = 0x41000000`, `r1 = 0xf00000` (taille,
  ~15 Mo) et appelle une fonction à `0x8400eeb4` (vérification,
  retourne « échec » si nul), puis, si elle réussit, une seconde à
  `0x84011ce4` avec les mêmes `r0`/`r1` (très probablement
  parsing+validation IMG3+déchiffrement). **`go` lit depuis exactement
  la même adresse (`0x41000000`) que celle vers laquelle pointe notre
  `run` patché (`0x41000001`, bit Thumb posé)**. Ce n'est donc pas
  seulement une hypothèse cohérente comme écrit plus haut dans la soirée :
  **`0x41000000` est confirmé être le tampon RAM générique où atterrit
  tout ce qu'on envoie par `send_data()`, utilisé aussi bien par la
  validation normale de `go` que par le saut direct, sans validation,
  de notre `run` patché.**

**Indice partiel (pas une preuve complète)** : les deux handlers déjà
désassemblés (`reset`/`reboot` à `0x84000cdc`, `go` à `0x84000d7c`) ont
chacun un prologue Thumb normal, `push {r7,lr}` et
`push {r4,r7,lr}; sub sp,#0x10` respectivement. Les deux supposent donc
une pile valide et utilisable à l'entrée, et un `LR` qui pointe vers un
point de retour sain (probablement le lecteur de commandes d'iBoot,
puisque ce sont des handlers de commandes console appelés par un
dispatcher commun). Si notre point d'entrée patché (`run`, même
mécanisme d'appel que ces deux-là) suit la même convention, ce qui n'est
**pas encore confirmé** (le dispatcher lui-même, celui qui fait le `BLX` vers
le handler, n'a pas été localisé ce soir, recherché par pointeur vers le
début de la table sans succès), alors un premier programme de test
pourrait être une fonction Thumb banale (`push`/`pop` standard) qui fait
quelque chose d'observable puis **retourne normalement**, plutôt que de
boucler indéfiniment. C'est une propriété de sécurité utile pour un premier
essai : on ne risque pas de laisser l'appareil pendu si le retour marche
comme pour les deux autres handlers.

### Pré-étude NotebookLM, 2026-10-14 (nuit) : confirme sans ajouter d'info nouvelle

Corpus déposé dans NotebookLM : toute la doc `docs/` + les notes de
travail du projet (non publiées) + les sources `dfuexec.py`/`recovery.py`/`image3.py`
d'ipwndfu (bundle à plat, non versionné). Réponse : elle confirme qu'elle n'a
**pas** le dispatcher de commandes dans son corpus (normal, on ne l'a
pas trouvé nous-mêmes, donc on ne pouvait pas le lui fournir), ne
fabrique rien pour combler le vide, et reformule correctement les deux
points déjà établis plus haut (convention d'appel cohérente par
analogie avec les deux handlers déjà désassemblés ; risque
seulement si le programme touche MMU/caches/pile/horloges/IRQ avant de
retourner). Un point confirmé en creux : **R0-R3 à l'entrée du
handler ne sont pas documentés** (ni par nous, ni par elle). Notre
futur payload de test ne doit pas en dépendre.

### Primitive de lecture existante trouvée : `getenv`

Recherche de chaînes dans le binaire déchiffré (`/tmp/ibss_decrypted.bin`,
même session) : présence de `getenv\x00` (offset corps `0x173c4`) dans
la zone des commandes console, **déjà confirmée fonctionnelle
aujourd'hui en Recovery** (`build-version`, `build-style`,
`config_board` lus avec succès plus tôt dans la journée, avant même
cette investigation). Pas de `md`/`dump`/`peek`/lecture mémoire
générique trouvée (build RELEASE, cohérent avec `build-style=RELEASE`
déjà lu) : `getenv`/`setenv` sont la seule primitive de lecture
disponible sans écrire nous-mêmes de code supplémentaire.

**Conséquence pour le premier test** : sans primitive de lecture
mémoire, on ne peut pas vérifier une preuve écrite en RAM arbitraire
après coup. Le test le plus sûr et le plus informatif qu'on puisse
faire *sans RE supplémentaire* : un payload Thumb qui ne touche à rien
(pas de MMU/caches/horloges/IRQ/pile autre que la sienne) et retourne
immédiatement, suivi d'un `getenv` de contrôle. Ce test répond à
« sauter à `0x41000001` et revenir casse-t-il quelque chose d'observable
? », pas à « notre code a-t-il tourné ». Un test positif
(preuve que le code s'exécute, pas juste qu'il ne casse rien) demanderait
de localiser une fonction d'affichage/d'écriture NVRAM interne par
désassemblage. Pas fait ce soir, chantier pour une prochaine session si
le test « ne casse rien » passe.

### Confirmé sur le matériel réel, 2026-10-14 (nuit) : premier code Trognon exécuté sur le 3GS

Exactement le test décrit ci-dessus, fait avec le go explicite de
l'utilisateur. Payload Thumb minimal (6 octets, `push {r7,lr}; nop;
pop {r7,pc}`, assemblé avec Keystone, vérifié par désassemblage
Capstone avant envoi, aucun octet deviné à la main), envoyé par
`recovery.send_data()` puis `recovery.send_command(device, 'run')`.
`getenv` donne les **mêmes valeurs avant et après**
(`build-version=iBoot-1072.61`, `config_board=n88ap`), et l'appareil
reste en Recovery sans redémarrer ni se déconnecter. Séquence reproductible : [`docs/en/01-boot-chain.md`](en/01-boot-chain.md).

Cela confirme en pratique tout ce qui précède dans cette section : le
tampon `0x41000000` reçoit les données envoyées, le patch `run`
saute à `0x41000001` en Thumb, la convention d'appel standard
(pile/LR) tient, et un retour via `pop {...,pc}` rend la main
proprement à la console d'iBoot. **Limite de cette preuve**, déjà
annoncée ci-dessus : elle démontre un aller-retour propre, pas
formellement que les 3 instructions se sont exécutées. Jugé peu
probable vu tout ce qui a été désassemblé et recoupé ce soir, mais pas
exclu sans une preuve positive (marqueur observable).

<details>
<summary>Ancienne version de cette section (fausse sur les deux points
ci-dessus, gardée pour traçabilité)</summary>

~~En relisant `dfuexec.py`'s `boot_ibss()` : les deux seuls patchs
appliqués à l'iBSS officiel sont au stade DFU... Rien ne patche le
chemin « Recovery vers `go` »... il faut localiser et patcher la routine de
validation, désassemblage requis, chantier à part.~~ **Faux** : voir
correction ci-dessus, `flash_nor()` prouve le contraire en code.

</details>
