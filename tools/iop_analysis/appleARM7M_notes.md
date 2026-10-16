# `AppleARM7M` / `IOSlaveProcessor` — IOP core notes

## État actuel (2026-10-16, soir — corrigé après relecture externe)

- `AppleARM7M::start()` and `AppleARM7M::startCPU(IOSlaveFirmware*)` **are
  already disassembled**, with real instructions, in
  [`docs/research/2026-10-16-wifi-iop-architecture.md`, section "The missing
  producer"](../../docs/research/2026-10-16-wifi-iop-architecture.md#the-missing-producer-everything-checked-for-the-real-sdhc-base-pointer).
  That is the primary source for both methods — this file does not
  re-derive it, only adds the few parts that section does not cover (the
  early steps of `start()`, before register-window mapping) and two small
  independent increments (below).
- The two IOP register windows (`0x86300000` index 0, `0xBF300000` index 1)
  were **already found and cross-confirmed** in the same research-doc
  section, independently via the driver's own disassembly and the device
  tree. Reproducible with [`dt_node_reg.py`](dt_node_reg.py) in this
  directory (self-contained, see its own docstring for the exact command
  and expected output).
- New tonight, not previously documented: the exact instance sizes of
  `IOSlaveProcessor.kext`'s six classes (below); `this+0x60` identified and
  confirmed as `_vicRegisterMap`'s cached virtual address (same confidence
  as the already-known `this+0x58`); a real register access through it
  found in a separate method (`0x80713510`) that writes `8` or `4` to
  offset `+0x18`; and a read-only probe payload for the `0xBF300000`
  window (`tools/payloads/trace.c`, STAGE 32, `trace_iop_vic.bin`), now
  updated to read `+0x18` specifically — not yet run on hardware.
- **This file's first version (earlier tonight) wrongly presented the
  already-known material above as new findings.** Corrected after an
  external review caught it; see "Historique" at the end for what changed
  and why, kept rather than deleted.

## Preuves

### The parts of `start()` not covered by the existing disassembly

The research doc's disassembly excerpt covers register-window mapping
(`mapDeviceMemoryWithIndex(0)` through caching `this+0x58`). It does not
cover the earlier steps of `start()` — provider cast, lock allocation,
`iop-version` read, interrupt registration, `function-device_reset`
resolution, optional JTAG enable. For those, the only evidence gathered
tonight is a set of `REQUIRE`-gated assertion strings, each carrying its
own line number from Apple's original source file:

```
AppleARM7M.cpp:70  NULL != (_provider = OSDynamicCast(AppleARMIODevice, provider))
AppleARM7M.cpp:71  true == IOService::start(_provider)
AppleARM7M.cpp:74  NULL != (_arm7mLock = IOLockAlloc())
AppleARM7M.cpp:78  tmpData->getLength() >= sizeof(UInt32)          [reads "iop-version"]
AppleARM7M.cpp:96  kIOReturnSuccess == _provider->registerInterrupt(0, this, ...&AppleARM7M::_interruptHandler)
AppleARM7M.cpp:97  NULL != (_arm7mRegisterMap = _provider->mapDeviceMemoryWithIndex(0))
AppleARM7M.cpp:97  NULL != (_vicRegisterMap = _provider->mapDeviceMemoryWithIndex(1))
AppleARM7M.cpp:106 NULL != _arm7mResetFunction
```

**Confiance : moyenne, pas établie.** Les numéros de ligne donnent une
**séquence candidate** cohérente avec l'ordre probable du code source —
ils ne prouvent ni les branches, ni que chaque étape s'exécute dans cet
ordre exact à l'exécution réelle. À confirmer par un vrai désassemblage
du corps de `start()` avant de s'appuyer dessus pour autre chose qu'une
intuition de structure.

**Effet matériel notable, lui aussi non prouvé par cette seule liste** :
une interruption est enregistrée (`registerInterrupt(0, ...)`). **Ça ne
prouve pas que l'IOP est actif** — l'enregistrement d'une ligne d'IRQ est
une étape de configuration côté CPU principal, pas un signal reçu. Même
si une IRQ était un jour effectivement reçue sur cette ligne, il
faudrait encore l'attribuer à une cause précise et vérifier qu'elle est
acquittée correctement avant d'y voir une preuve de vie de l'IOP.
`_interruptHandler` reste une bonne cible pour ça, pas encore localisé.

**Commande pour reproduire** :
```
r2 -e asm.bits=16 -c 'aaa; iz~str' /tmp/trognon_dt/AppleARM7M.kext.bin
```
(ré-extraire `AppleARM7M.kext.bin` d'abord — voir la méthode dans le
rapport de recherche cité plus haut ; jamais le binaire lui-même dans ce
dépôt.)

### `IOSlaveProcessor.kext` — tailles exactes des 6 classes (nouveau, 2026-10-16 soir)

Le rapport de recherche note déjà que ce kext (même SHA-256 vérifié ce
soir, `abff4df9877f8c83a899de4f2308bca80f556478790b982d1823d39fd366a0c9`)
« est presque entièrement du boilerplate IOKit générique » sans donner
les tailles exactes — c'est l'incrément réel de ce soir.

- **Localisation** : fonction d'initialisation du module, offset fichier
  `0x594` (vaddr `0x80399594`), qui appelle en boucle 6 paires de petites
  fonctions (allocation + constructeur `OSMetaClass`) — une par classe.
- **Décodage** : les fonctions examinées sont décodées en Thumb (pas une
  affirmation sur tout le binaire — un kext porte aussi des données et
  tables qui n'ont pas de mode d'instruction propre). Note pratique :
  r2 désassemble ce fichier de façon incohérente en mode ARM32 par
  défaut ; forcer `asm.bits=16` donne un résultat cohérent.
- **Nom original** : les 6 noms de classe (`IOSlaveMemoryBuffer`,
  `IOSlaveCPU`, `IOSlaveFirmware`, `IOSlaveMemory`, `IOSlaveEndpoint`,
  `IOSlaveProcessor`) sont des chaînes C littérales dans le binaire.
- **Confiance** : forte sur les tailles elles-mêmes (chaque constructeur
  charge `r3 = sizeof(Classe)` en littéral juste avant d'appeler le
  constructeur `OSMetaClass`, valeur directement lisible, pas déduite) :

| Classe | Taille (octets) |
|---|---|
| `IOSlaveMemory` | 8 |
| `IOSlaveMemoryBuffer` | 16 (`0x10`) |
| `IOSlaveEndpoint` | 56 (`0x38`) |
| `IOSlaveCPU` | 80 (`0x50`) |
| `IOSlaveFirmware` | 80 (`0x50`) |
| `IOSlaveProcessor` | 104 (`0x68`) |

**Interprétation** : confirme et précise ce que le rapport de recherche
dit déjà (boilerplate générique, pas de logique de chargement dans ce
kext) — n'ouvre pas de nouvelle piste, ferme plutôt une direction
(information valable en soi).

**Commande pour reproduire** :
```
r2 -e asm.bits=16 -c 'aaa; pdf @ 0x80399594' /tmp/trognon_dt/IOSlaveProcessor.kext.bin
```

### `this+0x60` tracé jusqu'à un accès registre réel (2026-10-16, soir, résolu)

Une troisième relecture a corrigé la piste : pas `this+0x7c/0x80/0x84/0x88`
(ces offsets sont vraisemblablement `_arm7mLock` ou un objet de
synchronisation voisin, voir plus bas — une fausse piste pour cette
question précise), mais `this+0x60`, par analogie structurelle directe
avec `this+0x58` déjà confirmé pour la première fenêtre. Vérifié, pas
supposé — chaîne complète ci-dessous.

**Dans `AppleARM7M::start(IOService*)`** (corps réel situé par recherche de
contexte autour des chaînes `REQUIRE` des lignes 96/97, pas par son
prologue directement — le prologue lui-même n'a pas été isolé ce soir) :

```
; index 0, déjà documenté dans le rapport de recherche :
ldr r0, [r4, 0x50]         ; provider
...
ldr.w r3, [r3, 0x208]      ; vtable+0x208 = mapDeviceMemoryWithIndex
blx r3                     ; mapDeviceMemoryWithIndex(0)
str r0, [r4, 0x54]         ; this+0x54 = IOMemoryMap #0

; index 1, pas documenté avant ce soir :
ldr r0, [r4, 0x50]         ; provider
movs r1, 1                 ; index = 1
ldr.w r3, [r3, 0x208]      ; même slot de vtable
blx r3                     ; mapDeviceMemoryWithIndex(1)
str r0, [r4, 0x5c]         ; this+0x5c = IOMemoryMap #1

; puis les deux adresses virtuelles, l'une après l'autre :
ldr r0, [r4, 0x54]
ldr r3, [r3, 0x4c]  ; vtable+0x4c = getVirtualAddress
blx r3
str r0, [r4, 0x58]          ; this+0x58 = adresse virtuelle fenêtre #0 (déjà connu)

ldr r0, [r4, 0x5c]
ldr r3, [r3, 0x4c]
blx r3
str r0, [r4, 0x60]          ; this+0x60 = adresse virtuelle fenêtre #1 -- NOUVEAU
```

Juste après, un seul appel de log (`"ARM7M::%s: mapped I/O registers at
%p/0x%08x, %p/0x%08x\n"`) reçoit **à la fois** `this+0x58` et `this+0x60`
en arguments — la preuve la plus directe possible que ces deux champs sont
une paire symétrique, fenêtre #0 et fenêtre #1, pas deux choses sans
rapport.

**`this+0x60` est donc l'adresse virtuelle de `_vicRegisterMap`, avec la
même confiance que `this+0x58` pour `_arm7mRegisterMap`.**

**Un vrai accès registre trouvé à travers lui**, dans une fonction séparée
(adresse de code `0x80713510`, pointeur Thumb appelable `0x80713511`,
présente comme entrée de vtable référencée depuis `0x807143f0` — donc une
vraie méthode virtuelle, pas du code mort) :

```
mov r5, r0          ; this
mov r4, r1           ; arg entier
...
cbz r4, A            ; si arg == 0
cmp r4, 1 ; it ne ...
bne B                ; sinon si arg != 1, saute B (pas d'accès registre)
                      ; sinon (arg == 1), tombe dans le même bloc que arg==0 avec une autre valeur
A: ldr r2, [r5, 0x60]  ; this+0x60 = adresse virtuelle fenêtre #1
   movs r3, 8
   str r3, [r2, 0x18]   ; écrit 8 (mot, 32 bits) à fenêtre_1 + 0x18
   b B
(bloc jumeau, arg==1) :
   ldr r2, [r5, 0x60]
   movs r3, 4
   str r3, [r2, 0x18]   ; écrit 4 à fenêtre_1 + 0x18
B: ldr r0, [r5, 0x7c]   ; verrou (voir ci-dessous)
   blx <fonction externe 0x800670b0>
   ...
   return 0
```

**Interprétation** : une méthode à un argument (0 ou 1) qui écrit soit `8`
soit `4` (deux bits distincts, `1<<3` et `1<<2`) sur un mot 32 bits à
l'offset `+0x18` de la fenêtre VIC — un patron très courant pour un
registre d'activation/masquage d'interruption à deux voies (deux lignes,
ou activer/désactiver). **Hypothèse, pas confirmée** : pourrait être
`enableInterrupt(bool)`/`setInterruptAction` côté IOP, ou tout autre
registre de contrôle à cet offset — la seule chose établie est l'accès
lui-même (adresse, largeur, valeurs possibles), pas sa sémantique exacte.

**`this+0x7c`, vu dans trois fonctions distinctes avec le même appel
externe (`0x800670b0`) juste avant de retourner** : cohérent avec un
verrou (`_arm7mLock`, alloué dans `start()` ligne 74 via `IOLockAlloc()`)
acquis/relâché autour de l'accès registre, pas un champ propre à la
fenêtre VIC. **Hypothèse raisonnable, pas vérifiée nom pour nom** (le
nom `_arm7mLock` vient de la chaîne `REQUIRE`, pas d'un désassemblage qui
le relierait explicitement à `this+0x7c`).

**Conséquence pour `trace_iop_vic.bin` (STAGE 32)** : le payload a été mis
à jour pour lire explicitement l'offset `+0x18` en plus du balayage
générique, puisque c'est maintenant un accès **confirmé par le pilote**,
pas une supposition.

**Commande pour reproduire** :
```
r2 -e asm.bits=16 -c 'aaa; pd 90 @ 0x80713190' /tmp/trognon_dt/AppleARM7M.kext.bin   # start(), les deux mapDeviceMemoryWithIndex
r2 -e asm.bits=16 -c 'aaa; pd 30 @ 0x80713510' /tmp/trognon_dt/AppleARM7M.kext.bin   # la fonction qui écrit +0x18
```

## `startCPU` lue en entier (2026-10-16)

Corps complet de `0x80713664` à `0x80713786` (un seul `pop {pc}`), pool de
littéraux `0x80713788`–`0x807137b0`. Fiche : `catalog/AppleARM7M/startCPU.yaml`
(`complete` = borné au graphe reconstruit ; les 11 appels indirects ou
externes y sont chacun listés avec la preuve qui les résoudrait).

Séquence, `r4 = this`, fenêtre 0 = `this+0x58` :

| Adresse | Effet |
|---|---|
| `0x80713672` | verrou `this+0x7c` via `0x800670b0` |
| `0x80713674`–`0x8071367c` | octet `this+0x80` non nul : retour `0xe00002d5`, aucun registre touché |
| `0x80713686` | `md = fw->vtbl[+0x34c]()` |
| `0x80713694` | `this+0x88 = md->vtbl[+0x98](0, &segLength)` |
| `0x807136a2`–`0x807136a8` | `md->vtbl[+0x84]()` (getLength) comparé à `segLength`, sinon REQUIRE ligne 155 |
| `0x807136c8`–`0x807136d6` | `this+0x8c` = plus petite puissance de 2 `>= max(0x1000, longueur)` |
| `0x807136e4`, `0x807136f4` | deux appels virtuels sur le provider (`+0x360`, `+0x364`) |
| `0x807136fe` | appel virtuel sur l'objet `this+0x68` (`+0x54`) |
| `0x80713704` | **`window0[+0x100] = 0x10`** |
| `0x8071370a`–`0x80713712` | relit `+0x100`, teste le bit 1 ; clair : message ligne 170 « not stopped for some reason », le code continue |
| `0x8071371e` | **`window0[+0x118] = 0x86300000`** (littéral) |
| `0x80713728` | **`window0[+0x110] = this+0x88`** |
| `0x80713740` ou `0x8071374c` | **`window0[+0x114]`** = `(-taille) & 0x3ffff000` si `this+0x64 == 1` (c'est le cas : `iop-version=1`), sinon `taille` |
| `0x80713752`–`0x8071375e` | si `this+0x6c` non nul : appel virtuel (`+0x50`, 0,0,0) |
| `0x80713768` | **`window0[+0x100] = 1`**, dernière écriture registre |
| `0x8071376e` | `this+0x80 = 1` |
| `0x80713778` | appel virtuel provider `+0x220`, puis déverrouillage `0x80067250`, retour 0 |

**Correction de la fiche précédente** : `ARM7M-STARTCPU-002` disait que la
fonction écrit « les octets bruts / la longueur du firmware » en
`+0x110/+0x114/+0x118`. Faux : `+0x118` est un littéral, `+0x110` un
résultat d'appel virtuel, `+0x114` un champ de taille. Aucun octet du
firmware n'est écrit par cette fonction ; la copie du firmware est faite
ailleurs (non localisée). La phrase d'origine est gardée ici, pas dans la
fiche, selon la règle de ce fichier.

**Ce que la lecture ne dit pas** : qui copie l'image, ce que font les
sept appels indirects entre les lignes ci-dessus, et ce que signifient les
bits de `+0x100`. Le firmware `fwFirmwareImage.bin` est une image plate liée
à l'adresse 0 (table de vecteurs `ea00000e e59ff018 ...`, adresses de
handlers absolues petites comme `0x40`, `0x939c`, aucun littéral `0x86xxxxxx`
dans les 110592 octets) : `0x86300000` n'est donc pas son adresse de
chargement dans la vue de l'IOP.

**Commande pour reproduire** :
```
r2 -e asm.bits=16 -c 'pd 90 @ 0x80713664' /tmp/trognon_dt/AppleARM7M.kext.bin
r2 -e asm.bits=16 -c 'px 32 @ 0x807137a0' /tmp/trognon_dt/AppleARM7M.kext.bin   # littéraux 0x86300000 et 0x3ffff000
```

## Prochaine couche (pas encore écrite)

Ce fichier et le rapport de recherche sont des notes d'analyse — la
structure interne des classes Apple (`this+0x58`, tailles d'objet, etc.)
y a sa place parce qu'elle sert la preuve. Une future référence de
composant (niveau « spec », voir `docs/spec/`) décrirait plutôt le
**contrat matériel compris** (quelles adresses, quelle séquence, quelles
garanties) sans reprendre le détail des classes C++ Apple — pas écrite ce
soir, à faire quand ce chantier se stabilise.

## Historique daté des corrections

- **2026-10-16, soir (version initiale de ce fichier)** : présentait
  `AppleARM7M::start()`/`startCPU()` comme non désassemblées, et la
  fenêtre `0xBF300000` comme une découverte de la session — les deux
  étaient déjà établies plus tôt le même jour (commit `ccf304f` et
  suivants, 09:49, dans le rapport de recherche) sans que je l'aie
  revérifié avant d'écrire. Toute la section « État actuel » et « Preuves »
  ci-dessus a été réécrite après une relecture externe qui a repéré
  l'écart. Gardé ici plutôt que supprimé, pour ne pas perdre la trace de
  l'erreur elle-même — voir la raison de la règle en tête de fichier
  (une affirmation invalidée ne doit pas rester dans le corps d'une
  fiche, elle doit migrer ici).
- **2026-10-16, soir (même relecture)** : les commandes de reproduction du
  nouveau script `dt_node_reg.py` ont remplacé un script ad hoc non
  committé qui citait un chemin temporaire — corrigé pour être
  autonome (chemin en paramètre, sortie attendue documentée dans le
  script lui-même).
- **2026-10-16, soir (troisième relecture)** : la piste précédente
  (`this+0x7c/0x80/0x84/0x88` dans `startCPU`) était une fausse piste pour
  cette question précise — gardée ci-dessus dans la section « entry point
  located but not fully traced » comme trace de ce qui a été cherché et
  écarté, pas supprimée. La bonne piste, suggérée par relecture externe
  (par analogie structurelle avec `this+0x58` déjà confirmé) et vérifiée
  ce soir par désassemblage réel, est `this+0x60` dans `start()` — qui
  mène à un accès registre réel (`+0x18`, écriture de `8` ou `4`) dans une
  méthode séparée (`0x80713510`), pas dans `startCPU`. Voir la nouvelle
  section ci-dessus, « `this+0x60` tracé jusqu'à un accès registre réel ».
  STAGE 32 mis à jour en conséquence pour lire `+0x18` spécifiquement.
