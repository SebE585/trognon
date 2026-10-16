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
  `IOSlaveProcessor.kext`'s six classes (below), and a read-only probe
  payload for the `0xBF300000` window (`tools/payloads/trace.c`, STAGE 32,
  `trace_iop_vic.bin`) — not yet run on hardware.
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

### `startCPU`'s entry point, located but not fully traced (2026-10-16, soir, suite)

Suite à une relecture externe demandant les offsets réellement accédés par
le pilote sur la seconde fenêtre (`_vicRegisterMap`, index 1) avant de
sonder à l'aveugle : recherche faite, **pas aboutie ce soir**.

- **Trouvé** : l'adresse d'entrée réelle de `startCPU` —
  **`0x80713665`** dans `AppleARM7M.kext.bin`, Thumb — confirmée en
  suivant la référence à la chaîne `"segLength >= md->getLength()"` /
  `AppleARM7M.cpp:155`, qui n'apparaît que dans le vrai corps de la
  fonction (distincte du stub d'appel virtuel pur à `0x80713e80`, qui ne
  fait que charger l'adresse de la chaîne de signature sans y faire
  grand-chose d'autre). Le rapport de recherche dit déjà avoir localisé et
  désassemblé `startCPU` mais ne donne pas cette adresse d'entrée
  précise — c'est l'ajout réel de cette recherche.
- **Pas trouvé ce soir** : quels offsets de `this` (ou d'un pointeur mis
  en cache depuis `_vicRegisterMap`) `startCPU` lit ou écrit
  spécifiquement. Les premières instructions tracées après l'entrée
  touchent `this+0x7c`/`+0x80`/`+0x84`/`+0x88` — des offsets différents de
  ceux déjà confirmés pour `_arm7mRegisterMap` (`this+0x58`,
  `+0x100`/`+0x110`/`+0x114`/`+0x118` relatifs à la fenêtre elle-même) —
  mais leur rôle exact (longueur du firmware ? un accesseur sur
  `IOSlaveFirmware` ? vraiment liés à la seconde fenêtre ou à autre
  chose ?) n'a pas été établi avant la fin de la session.
- **Conséquence pratique** : le payload `trace_iop_vic.bin` (STAGE 32)
  reste un **balayage à l'aveugle** des 8 premiers mots, explicitement
  documenté comme tel dans son propre commentaire — pas informé par les
  accès réels du pilote, faute d'avoir terminé cette trace. À ne pas
  présenter comme « ce que le pilote lit » tant que ce travail n'est pas
  terminé.

**Prochaine étape concrète pour clore ce point** : continuer le
désassemblage à partir de `0x80713665` (voir `pd` sur cette adresse) pour
identifier lesquels, parmi `this+0x7c/0x80/0x84/0x88`, correspondent
réellement à `_vicRegisterMap`/son adresse virtuelle mise en cache, puis
lesquels de ces champs sont effectivement lus/écrits à travers lui.

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
