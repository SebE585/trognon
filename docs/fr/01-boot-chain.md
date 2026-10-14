# 1. Chaîne de démarrage : du DFU à notre code

Objectif : exécuter du code écrit par nous sur un 3GS, sans rien écrire dans le stockage du téléphone.

## Le chemin (DEVICE, 2026-10-14/15)

![Chaîne de démarrage du iPhone 3GS](../img/boot-chain.svg)

```mermaid
sequenceDiagram
    participant H as Hôte (Mac)
    participant R as SecureROM (DFU)
    participant I as iBSS (patché, en RAM)
    participant P as Notre payload
    H->>R: ipwndfu -p (débordement de tas limera1n)
    R-->>H: DFU pwned (vérifications de signature coupées)
    H->>R: boot_ibss() : iBSS patché par DFU
    R->>I: saut, l'appareil se ré-énumère en Recovery 0x1281
    H->>I: send_data(payload) vers 0x41000000
    H->>I: send_command("run")
    I->>P: saut à 0x41000001 (Thumb)
    P-->>H: résultat par variable d'environnement (getenv)
```

1. **DFU pwned.** La bootrom du 3GS S5L8920 est vulnérable à limera1n. Hôte direct, sans carte
   auxiliaire. L'exploit échoue de temps en temps et se relance simplement.
2. **iBSS patché.** `boot_ibss()` d'ipwndfu envoie un iBSS déchiffré avec la clé de l'appareil et
   patché pour que la commande Recovery `run` saute à `0x41000001`. La commande `go` n'est pas utilisée.
3. **Payload.** `send_data` copie du Thumb brut en `0x41000000`, `run` y saute. État d'entrée : MMU
   et caches actifs, vecteurs en VA 0 (l'image iBSS à `0x84000000`), mode SVC,
   `SCTLR = 0x00C5187D`, IRQ non masquées.
4. **Canal de retour.** Le payload appelle `env_set()` interne de l'iBSS (`0x84011799`, Thumb) pour
   publier un marqueur, relu avec `getenv`. **Piège :** l'iBSS garde la variable d'un run à l'autre.
   La lire vide *avant* le run, sinon une valeur périmée ne prouve rien
   (voir les [notes de terrain](05-field-notes.md)).

## Ce qui ne persiste pas

Rien. L'exploit bootrom et le payload en RAM disparaissent au redémarrage (un logo signifie que
l'appareil a quitté la Recovery et doit être remis en DFU). C'est un démarrage captif. Le rendre
autonome sur ce SoC est une **HYPOTHESIS** : voir
[`../research/2026-10-13-untethered-boot.md`](../research/2026-10-13-untethered-boot.md).

## Reproduire

```sh
cd ~/tools/ipwndfu                       # une copie d'ipwndfu
export TROGNON_LIBUSB_PATH=/opt/homebrew/lib/libusb-1.0.dylib PYTHONPATH=.
python3 ipwndfu -p                        # relancer si « Exploit failed »
python3 trognon_boot_ibss_only.py         # aide locale : boot_ibss() puis arrêt
python3 <repo>/tools/run_payload.py <repo>/tools/payloads/trace.bin
```

`trognon_boot_ibss_only.py` est une enveloppe locale de 20 lignes autour de `boot_ibss()` d'ipwndfu ;
elle ne fait pas partie d'ipwndfu et n'est pas encore dans ce dépôt.
