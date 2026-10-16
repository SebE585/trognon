# `AppleARM7M.kext` — IOP core startup, read from the extracted kext

Started 2026-10-16 (late session), as the first application of the per-function
fiche format below (agreed with the user that evening): one card per analysed
function, confidence and name provenance (Apple's own vs. ours) stated
explicitly, raw decisive bytes kept next to the interpretation so the
interpretation can be checked independently of it.

**History rule for this file**: when a finding here is corrected, the old text
stays and a dated correction is appended under it — never silently rewritten.
Same rule `docs/research/2026-10-16-wifi-iop-architecture.md` already follows
(see its own "MAJOR CORRECTION" block).

Never the firmware/kext binary itself here — only our own analysis, same rule
as `fwFirmwareImage.r2` in this directory. Re-extract `AppleARM7M.kext.bin`
yourself from the owner's IPSW if you need to re-run anything below.

## What this kext is

The IOKit driver that binds to the `iop` device-tree node
(`compatible = "iop,s5l8920x"`) and owns the ARM7M core itself — a different
kext from `AppleIOPSDIO` (the SDIO *client* of the IOP, already analysed in
`fwFirmwareImage.r2`/the research report) and from `IOSlaveProcessor` (the
generic base classes `IOSlaveCPU`/`IOSlaveFirmware`/etc., see below). This is
the class that actually talks to the ARM7M's own hardware: its register
window, its own interrupt line, and (per its own method signature, not yet
disassembled) a `startCPU(IOSlaveFirmware*)` entry point.

---

### `AppleARM7M::start(IOService*)`

- **Binaire** : `AppleARM7M.kext.bin`, 12288 octets, extrait du kernelcache déchiffré de cet appareil (IPSW du propriétaire), SHA-256 `f286d9fb9b05f3802dbe2bccef2a4e329bcddd41ee85748d7fadc0789a7fa2e1`.
- **Localisation** : chaîne de signature à l'offset fichier `0x200c` (`"virtual bool AppleARM7M::start(IOService*)"`) ; corps de la fonction pas encore localisé/désassemblé ce soir — seule la **séquence**, reconstruite à partir des chaînes `REQUIRE` qu'elle contient, est établie ici.
- **Décodage** : Thumb (le binaire entier est Thumb ; confirmé par les autres fonctions de ce kext et par convention ARMv7 iOS de cette génération).
- **Nom original** : `AppleARM7M::start`, confirmé par la chaîne de signature C++ elle-même (pas une supposition).
- **Nom attribué** : aucun, nom Apple directement lisible.
- **Confiance** : forte sur la *séquence des étapes* (chaque étape correspond à une chaîne `REQUIRE failed` avec son propre numéro de ligne du fichier source original d'Apple, `AppleARM7M.cpp` — dix chaînes distinctes, lignes 70 à 106, qui donnent l'ordre très probable des vérifications) ; **faible sur l'implémentation exacte** (pas d'instructions désassemblées ce soir, seulement les chaînes qu'elles référencent).

**Entrées** : `IOService* provider` (le nœud device-tree `iop`, via `AppleARMIODevice`).

**Sorties** : `bool` (succès/échec du démarrage du service).

**Effets matériels (déduits, pas lus sur l'appareil)** :
- Enregistre une interruption matérielle (`_provider->registerInterrupt(0, this, ...AppleARM7M::_interruptHandler)`) — l'IOP a donc sa propre ligne d'IRQ gérée par ce pilote.
- Mappe **deux** régions de mémoire périphérique distinctes du même provider : index 0 (`_arm7mRegisterMap`) et index 1 (`_vicRegisterMap`) — voir la fiche séparée ci-dessous, c'est la vraie trouvaille de ce soir.
- Lit la propriété device-tree `iop-version` (4 octets, `tmpData->getLength() >= sizeof(UInt32)`).
- Récupère une fonction de reset via `function-device_reset` (device tree) dans `_arm7mResetFunction`.
- Optionnellement active le JTAG via `function-jtag_enable` si la propriété `arm7m-enable-jtag` est présente.

**Appelants et fonctions appelées** : appelé par IOKit au matching du provider (standard `IOService::start`), pas encore tracé côté appelant dans ce binaire. Appelle `IOService::start(_provider)` (classe parente), `IOLockAlloc()`, `_provider->mapDeviceMemoryWithIndex()` (x2), `_provider->registerInterrupt()`.

**Instructions décisives** : aucune encore extraite en octets ce soir — seulement les dix chaînes `REQUIRE`, listées ici dans l'ordre de leur propre numéro de ligne source (pas l'ordre d'apparition dans le binaire, qui n'est pas nécessairement le même) :

```
AppleARM7M.cpp:70  NULL != (_provider = OSDynamicCast(AppleARMIODevice, provider))
AppleARM7M.cpp:71  true == IOService::start(_provider)
AppleARM7M.cpp:74  NULL != (_arm7mLock = IOLockAlloc())
AppleARM7M.cpp:78  tmpData->getLength() >= sizeof(UInt32)          [lecture de "iop-version"]
AppleARM7M.cpp:92  (pas de chaîne de condition courte associée ici)
AppleARM7M.cpp:96  kIOReturnSuccess == _provider->registerInterrupt(0, this, ...&AppleARM7M::_interruptHandler)
AppleARM7M.cpp:97  NULL != (_arm7mRegisterMap = _provider->mapDeviceMemoryWithIndex(0))
AppleARM7M.cpp:97  NULL != (_vicRegisterMap = _provider->mapDeviceMemoryWithIndex(1))   [même ligne source, deux REQUIRE]
AppleARM7M.cpp:106 NULL != _arm7mResetFunction
```

**Interprétation** : `start()` configure l'IOP comme un vrai périphérique IOKit standard — verrou, lecture de version, enregistrement d'IRQ, mapping mémoire, résolution de la fonction de reset — avant qu'un `startCPU()` séparé (fiche ci-dessous) ne le démarre réellement. L'ordre exact des étapes ci-dessus est une reconstruction à partir des numéros de ligne, pas une trace d'exécution.

**Questions ouvertes** :
- Désassembler le vrai corps de `start()` pour confirmer l'ordre et voir ce qui se passe entre les lignes 78 et 92 (pas de chaîne connue dans cet intervalle ce soir).
- `_interruptHandler` (le vrai gestionnaire d'IRQ de l'IOP côté CPU principal) n'est pas encore localisé — ce serait un candidat direct pour un marqueur « l'IOP est réellement actif » (étape « Exécution » du chantier IOP).

**Commande pour reproduire** : `r2 -e asm.bits=16 -c 'aaa; iz~str' /tmp/trognon_dt/AppleARM7M.kext.bin` (ré-extraire le kext d'abord, voir le research report pour la méthode).

---

### La fenêtre VIC propre à l'IOP — 0xBF300000 (SOURCE, nouveau ce soir)

- **Localisation** : propriété `reg` du nœud device tree `/device-tree/arm-io/iop`, deuxième paire (adresse, taille) : `0x3F300000, 0x1000`. Décodée ce soir (2026-10-16) avec le même script `parse_dt.py` déjà utilisé pour Wi-Fi/Bluetooth, en le adaptant pour filtrer sur `iop` au lieu de `sdio`/`bluetooth`.
- **Traduction en adresse CPU** : confirmée en relisant la propriété `ranges` de `/device-tree/arm-io` elle-même (`[(enfant=0x0, parent=0x80000000, taille=0x40000000), (enfant=0x40000000, parent=0x40000000, taille=0x40000000)]`, la même règle déjà établie pour `0x06300000 -> 0x86300000`) : `0x3F300000` est sous `0x40000000`, donc `+0x80000000` -> **`0xBF300000`**.
- **Nom original** : aucun nom Apple direct dans le device tree pour cette seconde plage ; le nom vient du code du pilote (`_vicRegisterMap`, voir la fiche `start()` ci-dessus), lui-même une variable membre nommée par le développeur Apple d'origine, lisible dans la chaîne `REQUIRE`.
- **Nom attribué** : "IOP VIC" dans nos propres notes, par analogie avec les VIC0/1/2 déjà confirmés sur l'appareil (`0xBF2x0000`) — `0xBF300000` suit exactement le même schéma d'adresse (`0xBF1xxxxx` = timer, `0xBF2xxxxx` = VIC0-2, `0xBF3xxxxx` = cette nouvelle fenêtre), ce qui rend l'hypothèse « c'est un contrôleur d'interruptions » plausible mais **pas confirmée par une lecture de registre** — seulement par la convention de nommage du pilote (`_vicRegisterMap`) et par la similarité d'adressage.
- **Confiance** : **forte** sur l'existence et l'adresse (deux sources indépendantes concordent : device tree `reg` et code du pilote `mapDeviceMemoryWithIndex(1)`/`_vicRegisterMap`) ; **non confirmée** sur le rôle précis (VIC générique vs. quelque chose de plus spécifique à l'ARM7M) tant que rien n'est lu sur le matériel.

**Effets matériels** : aucun — pur SOURCE, jamais sondé.

**Questions ouvertes** : un relevé en lecture seule (même patron que STAGE 26/27/28 pour la fenêtre de registres principale de l'IOP) donnerait un premier signal : tout à zéro comme la fenêtre principale, ou une structure de registre ARM VIC reconnaissable (comme VIC0/1/2 déjà confirmés) ?

**Commande pour reproduire la découverte elle-même** : voir le script Python ad hoc utilisé ce soir (basé sur `parse_dt.py`, filtré sur le nœud `iop` plutôt que `sdio`/`bluetooth` ; à committer dans ce dossier si repris).

---

### Enregistrement des 6 classes d'`IOSlaveProcessor.kext` (mineur, boilerplate)

- **Binaire** : `IOSlaveProcessor.kext.bin`, 12288 octets.
- **Localisation** : fonction d'initialisation du module à l'offset `0x594` (vaddr `0x80399594`), qui appelle en boucle 6 paires de petites fonctions (allocation + constructeur OSMetaClass) — une par classe.
- **Décodage** : Thumb (confirmé — le binaire est mal désassemblé en mode ARM32 par défaut dans r2, correctement en forçant `asm.bits=16`, à noter pour la prochaine session).
- **Nom original** : les 6 noms de classe (`IOSlaveMemoryBuffer`, `IOSlaveCPU`, `IOSlaveFirmware`, `IOSlaveMemory`, `IOSlaveEndpoint`, `IOSlaveProcessor`) sont des chaînes C littérales dans le binaire, pas une supposition.
- **Nom attribué** : aucun.
- **Confiance** : forte sur les tailles d'objet (chaque constructeur charge `r3 = sizeof(Classe)` juste avant d'appeler le constructeur `OSMetaClass`, valeur directement lisible) : `IOSlaveMemory`=8, `IOSlaveMemoryBuffer`=16 (`0x10`), `IOSlaveEndpoint`=56 (`0x38`), `IOSlaveCPU`=80 (`0x50`), `IOSlaveFirmware`=80 (`0x50`), `IOSlaveProcessor`=104 (`0x68`) octets.

**Interprétation** : pur enregistrement RTTI/OSMetaClass au chargement du kext, le même motif que dans n'importe quel kext IOKit. Ce binaire ne contient **pas** la logique de chargement de firmware elle-même (rien dans ses 2968 octets de `__text` au-delà de ces 6 paires de constructeurs et un appel commun) — c'est un indice négatif utile : la vraie logique de chargement (qui copie les octets, à quelle adresse, quelle écriture démarre le cœur) doit être ailleurs, très probablement dans `AppleARM7M::startCPU(IOSlaveFirmware*)` (voir ci-dessus, pas encore désassemblée) plutôt que dans ce kext générique.

**Questions ouvertes** : aucune nouvelle piste ici — ce résultat ferme cette direction plutôt que d'en ouvrir une, ce qui est en soi une information (voir la méthode de travail : « une hypothèse éliminée » est un résultat de session valable).

**Commande pour reproduire** : `r2 -e asm.bits=16 -c 'aaa; pdf @ 0x80399594' /tmp/trognon_dt/IOSlaveProcessor.kext.bin`.
